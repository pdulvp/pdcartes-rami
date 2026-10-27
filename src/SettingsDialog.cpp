#include "SettingsDialog.h"

#include "FeltArt.h"
#include "CardDeck.h"
#include "DialogChrome.h"
// Généré par CMake à partir de data/links.ini ; voir CMakeLists.txt.
#include "Links.h"
#include "MenuStyle.h"
#include "Palette.h"
#include "TableLook.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kHeadingGap = 8;

// Liste des pages, à gauche. Sa largeur est ajustée au plus long des noms de
// page, l'espace restant étant dévolu au contenu.
constexpr int kSectionsWidth = 172;
constexpr int kSectionRowHeight = 30;

// Hauteur d'un aperçu de dos : suffisante pour que le motif soit
// identifiable, assez réduite pour que les dos d'un jeu tiennent sur une
// ligne.
constexpr int kBackHeight = 92;

// Hauteur des figures, inférieure à celle des dos : huit figures doivent
// tenir sur une ligne, alors qu'un dos est présenté seul.
constexpr int kFaceHeight = 78;
constexpr qreal kFaceGap = 5.0;

// Espacement entre les cartes et le bord du panneau qui les contient.
constexpr int kStripPad = 9;

QCheckBox *switchRow(QWidget *parent, const QString &text, bool on) {
    auto *box = new QCheckBox(text, parent);
    box->setChecked(on);
    // La couleur est imposée plutôt qu'héritée : un style de plateforme
    // attribue à une case à cocher une palette propre, qui prévaut sur celle du
    // corps de la boîte.
    QPalette ink = box->palette();
    ink.setColor(QPalette::WindowText, currentPalette().surfaceText);
    ink.setColor(QPalette::Text, currentPalette().surfaceText);
    box->setPalette(ink);
    return box;
}

QRadioButton *choiceRow(QWidget *parent, const QString &text) {
    auto *button = new QRadioButton(text, parent);
    QPalette ink = button->palette();
    ink.setColor(QPalette::WindowText, currentPalette().surfaceText);
    ink.setColor(QPalette::Text, currentPalette().surfaceText);
    button->setPalette(ink);
    return button;
}

// Libellé placé devant un champ, dans la couleur de texte du corps de la
// boîte.
QLabel *fieldLabel(QWidget *parent, const QString &text) {
    auto *label = new QLabel(text, parent);
    QPalette ink = label->palette();
    ink.setColor(QPalette::WindowText, currentPalette().surfaceText);
    label->setPalette(ink);
    return label;
}

// Ligne explicative placée sous le champ qu'elle décrit.
QLabel *asideLabel(QWidget *parent, const QString &text) {
    auto *label = new QLabel(text, parent);
    label->setWordWrap(true);
    markHintLabel(label);
    return label;
}

// Bouton d'icône, placé à droite du champ sur lequel il agit.
//
// Aperçu d'un dos, rendu exactement comme il le sera sur la table.
//
// Un `set` nul — ou un dos absent du jeu sélectionné — produit le treillis
// dessiné par le programme, qui est précisément ce qui serait affiché sur la
// table dans ce cas. L'aperçu reflète donc toujours le rendu réel.
QPixmap backPreview(const CardSet *set, int at, const QSizeF &box) {
    const qreal dpr = qApp->devicePixelRatio();
    QPixmap tile(QSize(int(std::ceil(box.width() * dpr)), int(std::ceil(box.height() * dpr))));
    tile.setDevicePixelRatio(dpr);
    tile.fill(Qt::transparent);

    QPainter painter(&tile);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF where(0.5, 0.5, box.width() - 1.0, box.height() - 1.0);
    const QPixmap art = set ? set->backImage(at, tile.size()) : QPixmap();
    if (art.isNull()) {
        CardLook look;
        look.noShadow = true;
        drawCardBack(&painter, where, currentPalette(), look);
        return tile;
    }

    const qreal corner = cardCornerFor(where);
    QPainterPath shape;
    shape.addRoundedRect(where, corner, corner);
    painter.setPen(Qt::NoPen);
    painter.fillPath(shape, currentPalette().cardBack);
    painter.setClipPath(shape);
    QRectF onto(0, 0, art.width() / dpr, art.height() / dpr);
    onto.moveCenter(where.center());
    painter.drawPixmap(onto, art, QRectF(0, 0, art.width(), art.height()));
    return tile;
}

// Main d'exemple présentée dans l'aperçu.
//
// Le joker figure en premier : c'est la carte qui varie le plus d'un jeu à
// l'autre, et celle qui vaut vingt-cinq points au rami. Viennent ensuite l'as
// et les trois figures, qui sont les cartes illustrées, puis quatre cartes
// numérales, qui montrent la disposition des symboles.
//
// Huit cartes portent une couleur, soit deux par couleur, le joker n'en ayant
// pas. Neuf cartes suffisent à caractériser un jeu ; les quatorze d'une main
// réelle ne pourraient être affichées à une taille lisible.
QVector<Card> sampleHand() {
    static const int ranks[] = {kAs, kRoi, kDame, kValet, 10, 7, 5, 3};
    static const Suit suits[] = {Suit::Pique, Suit::Coeur, Suit::Carreau, Suit::Trefle};
    QVector<Card> hand;

    Card joker;
    joker.id = 0;
    joker.rank = kJoker;
    hand.append(joker);

    for (int i = 0; i < 8; ++i) {
        Card card;
        card.id = i + 1;    // seulement pour qu'elle soit valide: rien ne la pointe
        card.rank = ranks[i];
        card.suit = suits[i % 4];
        hand.append(card);
    }
    return hand;
}

} // namespace

// Figures du jeu sélectionné, affichées côte à côte.
//
// Le principe est celui des dos, pour la même raison : le nom d'un jeu
// n'indique pas l'apparence de ses cartes, seule information recherchée. La
// sélection ne porte toutefois pas sur une carte mais sur le jeu entier, choisi
// dans la liste située au-dessus ; la bande est donc purement visuelle et ne
// répond ni au survol, ni à la sélection, ni au clavier.
//
// Elle est dessinée plutôt que construite comme une liste : une `QListWidget`
// dont aucun élément n'est sélectionnable présenterait une apparence trompeuse,
// alors qu'il ne s'agit ici que d'une rangée d'images à peindre.
class CardStrip : public QWidget {
public:
    explicit CardStrip(QWidget *parent = nullptr) : QWidget(parent), m_hand(sampleHand()) {
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFixedHeight(kFaceHeight + 2 * kStripPad);
    }

    QSize sizeHint() const override {
        const QSizeF card = cardSizeForHeight(kFaceHeight);
        const qreal wide = card.width() * m_hand.size() + kFaceGap * (m_hand.size() - 1);
        return QSize(int(std::ceil(wide)) + 2 * kStripPad, height());
    }

protected:
    void paintEvent(QPaintEvent *) override {
        const Palette &scheme = currentPalette();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        // Même panneau que la liste des dos, afin que les deux bandes
        // apparaissent de même nature.
        const QRectF panel = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath shape;
        shape.addRoundedRect(panel, 8, 8);
        painter.setPen(Qt::NoPen);
        painter.fillPath(shape, scheme.field);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(scheme.edge, 1.0));
        painter.drawPath(shape);

        const int count = m_hand.size();
        if (count <= 0) return;

        // Hauteur souhaitée et hauteur effectivement possible. Un jeu installé
        // impose sa propre proportion — les fichiers CARDS.DLL de Windows sont
        // plus larges que les cartes dessinées — et la même rangée de huit
        // cartes y occupe une largeur supérieure.
        const qreal room = panel.width() - 2 * kStripPad - kFaceGap * (count - 1);
        const qreal fits = cardSizeForWidth(room / count).height();
        const QSizeF card = cardSizeForHeight(std::max(24.0, std::min(qreal(kFaceHeight), fits)));

        const qreal span = card.width() * count + kFaceGap * (count - 1);
        qreal x = panel.center().x() - span / 2.0;
        const qreal y = panel.center().y() - card.height() / 2.0;

        CardLook look;
        look.noShadow = true;   // elles sont sur le corps de la boîte, pas sur le tapis
        for (const Card &one : m_hand) {
            drawCardFace(&painter, QRectF(x, y, card.width(), card.height()), one, scheme, look);
            x += card.width() + kFaceGap;
        }
    }

private:
    QVector<Card> m_hand;
};

SettingsDialog::SettingsDialog(const GameSettings &settings, QWidget *parent)
    : QDialog(parent), m_settings(settings) {
    setWindowTitle(tr("Réglages"));

    const DialogChrome chrome = applyDialogChrome(this, tr("Réglages"));

    // La liste des pages et les pages elles-mêmes. La liste est plate, les
    // cinq groupes ne comportant pas de sous-niveau.
    m_sections = new QTreeWidget(this);
    m_sections->setFixedWidth(kSectionsWidth);
    m_sections->setHeaderHidden(true);
    m_sections->setRootIsDecorated(false);
    m_sections->setFrameShape(QFrame::NoFrame);   // la feuille de style dessine le bord
    m_sections->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_sections->setFocusPolicy(Qt::NoFocus);

    m_pages = new QStackedWidget(this);

    auto *columns = new QHBoxLayout;
    columns->setSpacing(kDialogMargin);
    columns->addWidget(m_sections);
    columns->addWidget(m_pages, 1);
    chrome.fields->addLayout(columns, 1);

    // Les pages sont classées par ordre d'importance : les paramètres
    // affectant le jeu d'abord, l'apparence ensuite, celle-ci n'ayant aucune
    // incidence sur la partie, et l'accessibilité en dernier.
    buildDuringPage();
    buildCardsPage();
    buildBacksPage();
    buildFeltPage();
    buildAccessPage();

    // Après la construction des pages et non à la fin de celle des cartes :
    // le remplissage de la liste des jeux remplit également celle des dos,
    // située sur la page suivante.
    fillDecks();
    dressLists();
    // Après la construction des pages également : les deux étiquettes de
    // lien n'existent qu'une fois leurs pages créées.
    dressEditorLinks();

    connect(m_sections, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *now, QTreeWidgetItem *) {
                if (now) m_pages->setCurrentIndex(now->data(0, Qt::UserRole).toInt());
            });
    m_sections->setCurrentItem(m_sections->topLevelItem(0));

    // --- Pied de la boîte.
    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(tr("Annuler"), QDialogButtonBox::RejectRole);
    QPushButton *ok = buttons->addButton(tr("Valider"), QDialogButtonBox::AcceptRole);
    markDefaultButton(ok);
    ok->setDefault(true);
    chrome.footer->addStretch();
    chrome.footer->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // La hauteur est celle qu'exige la page la plus remplie des cinq — celle
    // des dos, en raison de sa rangée de vignettes et de la notice qui la
    // suit — afin que le passage d'une page à l'autre ne redimensionne jamais
    // la fenêtre, l'espace excédentaire restant en bas des pages moins
    // remplies. Elle reste par ailleurs compatible avec un écran d'ordinateur
    // portable, ce que la colonne unique de huit cents pixels n'était pas.
    resize(780, 490);
}

QVBoxLayout *SettingsDialog::addPage(const QString &name, const QString &heading) {
    auto *page = new QWidget(m_pages);
    auto *rows = new QVBoxLayout(page);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(0);
    rows->addWidget(dialogSectionHeading(page, heading));
    rows->addSpacing(kHeadingGap);

    const int at = m_pages->addWidget(page);
    auto *row = new QTreeWidgetItem(m_sections, {name});
    row->setData(0, Qt::UserRole, at);
    row->setSizeHint(0, QSize(0, kSectionRowHeight));
    return rows;
}

void SettingsDialog::buildDuringPage() {
    QVBoxLayout *rows = addPage(tr("Pendant la partie"), tr("Pendant la partie"));
    QWidget *page = m_pages->widget(m_pages->count() - 1);

    m_autoSort = switchRow(page, tr("Ranger la main toute seule après chaque pioche"),
                           m_settings.autoSort);
    m_offerHints = switchRow(page, tr("Proposer le bouton Conseil"), m_settings.offerHints);
    m_celebrate = switchRow(page, tr("Feu d'artifice quand la partie est gagnée"),
                            m_settings.celebrate);
    rows->addWidget(m_autoSort);
    rows->addWidget(m_offerHints);
    rows->addWidget(m_celebrate);
    rows->addSpacing(14);

    auto *paceRow = new QHBoxLayout;
    paceRow->setSpacing(18);
    paceRow->addWidget(fieldLabel(page, tr("Les adversaires jouent :")));
    m_paces = new QButtonGroup(this);
    const QStringList paces = {tr("posément"), tr("normalement"), tr("vivement")};
    for (int i = 0; i < paces.size(); ++i) {
        auto *button = choiceRow(page, paces.at(i));
        button->setChecked(i == qBound(0, m_settings.pace, 2));
        m_paces->addButton(button, i);
        paceRow->addWidget(button);
    }
    paceRow->addStretch();
    rows->addLayout(paceRow);
    rows->addStretch(1);
}

// Les deux boutons accompagnant une liste de jeux : ouvrir le dossier
// d'installation, et le relire sans quitter la boîte.
//
// Ils sont ajoutés par une fonction plutôt que répétés sur chaque page : les
// figures et les dos disposent chacun de leur liste, toutes deux lisant le
// même dossier, et ne placer les boutons que sur l'une des deux obligeait à
// changer de page pour relire un jeu qui venait d'y être installé.
void SettingsDialog::addDeckFolderButtons(QWidget *page, QHBoxLayout *row) {
    auto *openFolder = iconButton(page,
                                  {QStringLiteral("folder-open-symbolic"),
                                   QStringLiteral("folder-symbolic"),
                                   QStringLiteral("document-open-symbolic"),
                                   QStringLiteral("folder")},
                                  tr("Dossier"), tr("Ouvrir le dossier des cartes"));
    connect(openFolder, &QToolButton::clicked, this, [] {
        CardDecks::ensureFolder();
        QDesktopServices::openUrl(QUrl::fromLocalFile(CardDecks::folder()));
    });
    auto *rescan = iconButton(page,
                              {QStringLiteral("view-refresh-symbolic"),
                               QStringLiteral("view-refresh")},
                              tr("Relire"), tr("Relire le dossier des cartes"));
    // Relire remplit les deux listes: celle des figures et celle des dos.
    connect(rescan, &QToolButton::clicked, this, [this] {
        CardDecks::instance().refresh();
        fillDecks();
    });
    row->addWidget(openFolder);
    row->addWidget(rescan);
}

// Ajoute le lien vers l'éditeur au bas d'une page.
//
// Le lien est posé par une fonction plutôt que répété sur chaque page, pour
// la même raison que les deux boutons ci-dessus : les figures et les dos
// proviennent de jeux que l'utilisateur installe, et l'origine de ces jeux
// est une question commune aux deux pages.
//
// Le texte n'est pas défini ici mais par `dressEditorLinks()`, qui connaît
// la couleur courante et qui est rappelée à chaque changement de teinte.
void SettingsDialog::addEditorLink(QWidget *page, QVBoxLayout *rows) {
    auto *link = asideLabel(page, QString());
    // Le lien est activable à la souris comme au clavier.
    link->setTextInteractionFlags(Qt::TextBrowserInteraction);
    link->setOpenExternalLinks(true);
    m_editorLinks.append(link);
    rows->addWidget(link);
}

// Écrit la ligne de lien des deux pages avec la couleur courante.
//
// La couleur de l'ancre est inscrite dans le texte lui-même, ce qui impose
// de réécrire celui-ci à chaque changement de teinte : la feuille de style
// de la fenêtre colore bien le texte des aides
// (`QLabel[dialogHint="true"]`), mais ne s'applique pas au texte enrichi.
// Sans cette couleur explicite, le lien s'afficherait dans le bleu par
// défaut du système, étranger à la palette de la fenêtre.
void SettingsDialog::dressEditorLinks() {
    // L'adresse est commune aux deux pages et provient de data/links.ini.
    const QString anchor = QStringLiteral("<a href=\"%1\" style=\"color: %2\">%1</a>")
                               .arg(cardEditorUrl(), currentPalette().accent.name());
    // L'adresse est placée sur sa propre ligne au moyen d'un saut explicite,
    // plutôt que laissée au retour automatique. Placée à la suite de la
    // phrase, elle dépassait la largeur disponible et se coupait après son
    // trait d'union ; la propriété `white-space: nowrap`, essayée, n'est pas
    // honorée par le moteur de texte enrichi de Qt. Or une adresse scindée
    // en deux lignes ne peut plus être recopiée de visu.
    const QString sentence = tr("Un éditeur de cartes est disponible ici :");
    for (QLabel *link : m_editorLinks) {
        link->setText(sentence + QStringLiteral("<br>") + anchor);
    }
}

void SettingsDialog::buildCardsPage() {
    QVBoxLayout *rows = addPage(tr("Les cartes"), tr("Les cartes"));
    QWidget *page = m_pages->widget(m_pages->count() - 1);

    auto *deckRow = new QHBoxLayout;
    deckRow->setSpacing(8);
    deckRow->addWidget(fieldLabel(page, tr("Jeu de cartes :")));
    m_deck = new QComboBox(page);
    deckRow->addWidget(m_deck, 1);

    addDeckFolderButtons(page, deckRow);
    rows->addLayout(deckRow);
    rows->addSpacing(4);

    m_deckNote = asideLabel(page, QString());
    rows->addWidget(m_deckNote);
    rows->addSpacing(8);

    // Les figures, tout de suite sous le jeu qu'on vient de choisir: c'est la
    // réponse à « à quoi celui-là ressemble-t-il », et elle est à sa place
    // contre la question.
    m_faces = new CardStrip(page);
    rows->addWidget(m_faces);
    rows->addSpacing(14);

    addEditorLink(page, rows);
    rows->addStretch(1);

    connect(m_deck, &QComboBox::currentIndexChanged, this, [this] {
        rememberDeck(m_deck->currentData().toString());
        // Le dos suit peut-être: « celui du jeu de cartes » vient de désigner
        // un autre jeu.
        fillBacks();
        describeDeck();
        m_faces->update();
    });
}

void SettingsDialog::buildBacksPage() {
    QVBoxLayout *rows = addPage(tr("Les dos"), tr("Les dos"));
    QWidget *page = m_pages->widget(m_pages->count() - 1);

    // Sa propre liste de jeux. Un jeu vient d'habitude avec ses dos et on les
    // prend avec lui -- c'est la première entrée, et c'est le cas ordinaire --
    // mais rien ne l'oblige: des figures d'un jeu et le dos d'un autre font un
    // jeu qui n'existe pas et qui se tient très bien.
    auto *deckRow = new QHBoxLayout;
    deckRow->setSpacing(8);
    deckRow->addWidget(fieldLabel(page, tr("Dos du jeu :")));
    m_backDeck = new QComboBox(page);
    deckRow->addWidget(m_backDeck, 1);
    addDeckFolderButtons(page, deckRow);
    rows->addLayout(deckRow);
    rows->addSpacing(10);

    // Les dos se choisissent sur l'image plutôt que sur leur nom: « Treillis »
    // ne dit pas à quoi un dos ressemble, et c'est la seule chose qu'on veut
    // savoir d'un dos.
    const int backWide = int(std::ceil(kBackHeight * kCardRatio));
    m_backs = new QListWidget(page);
    m_backs->setViewMode(QListView::IconMode);
    m_backs->setFlow(QListView::LeftToRight);
    m_backs->setWrapping(false);
    m_backs->setMovement(QListView::Static);
    m_backs->setSelectionMode(QAbstractItemView::SingleSelection);
    m_backs->setIconSize(QSize(backWide, kBackHeight));
    // Large de la vignette et d'un peu plus, et jamais moins que de quoi
    // écrire « celui du jeu » sur deux lignes: un nom de dos coupé net ne dit
    // plus lequel c'est.
    m_backs->setGridSize(QSize(std::max(backWide + 26, 104), kBackHeight + 32));
    m_backs->setSpacing(6);
    m_backs->setFrameShape(QFrame::NoFrame);
    m_backs->setWordWrap(true);
    m_backs->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_backs->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Juste la place d'une rangée de vignettes et de la barre qui glisse
    // dessous quand un jeu en apporte plus qu'il n'en tient.
    m_backs->setFixedHeight(kBackHeight + 32 + 22);
    rows->addWidget(m_backs);
    rows->addSpacing(6);
    rows->addWidget(asideLabel(
        page, tr("Le dos est ce qu'on voit de la pioche et des mains d'en face pendant "
                 "toute la manche. Un jeu installé apporte les siens ; sans eux, c'est le "
                 "treillis que le programme dessine, dans la couleur du tapis. Le dos "
                 "appartient au profil, comme le jeu de cartes.")));
    rows->addSpacing(6);
    addEditorLink(page, rows);
    rows->addStretch(1);

    connect(m_backDeck, &QComboBox::currentIndexChanged, this, [this] {
        rememberBackDeck(m_backDeck->currentData().toString());
        fillBacks();
    });
    connect(m_backs, &QListWidget::currentItemChanged, this,
            [](QListWidgetItem *now, QListWidgetItem *) {
                if (!now) return;
                const int which = now->data(Qt::UserRole).toInt();
                if (which >= 0) rememberBack(which);
            });
}

// La taille des cartes, seule sur sa page et en dernier dans la liste.
//
// Elle était le dernier réglage de la page des cartes, sous le jeu choisi, sa
// notice et la bande des figures -- or elle ne répond pas à la même question:
// le jeu de cartes est un goût, la taille est ce qui rend les figures
// lisibles à qui en a besoin. Une section nommée pour ce qu'elle sert se
// trouve en la cherchant par son nom ; un curseur rangé sous un titre qui
// parle d'autre chose se trouve en descendant la page.
//
// En dernier, après le décor: la liste va de ce qui touche à la partie vers
// ce qui touche à la vue.
void SettingsDialog::buildAccessPage() {
    QVBoxLayout *rows = addPage(tr("Accessibilité"), tr("Accessibilité"));
    QWidget *page = m_pages->widget(m_pages->count() - 1);

    auto *zoomRow = new QHBoxLayout;
    zoomRow->setSpacing(10);
    zoomRow->addWidget(fieldLabel(page, tr("Taille des cartes :")));
    m_zoom = new QSlider(Qt::Horizontal, page);
    m_zoom->setRange(80, 200);
    m_zoom->setSingleStep(10);
    m_zoom->setPageStep(10);
    m_zoom->setTickInterval(10);
    m_zoom->setValue(qBound(80, m_settings.zoom, 200));
    m_zoom->setFixedWidth(220);
    m_zoomNote = new QLabel(page);
    markHintLabel(m_zoomNote);
    auto sayZoom = [this] {
        // Arrondi au cran: la poignée se pose entre deux sans cela.
        const int at = int(std::round(m_zoom->value() / 10.0)) * 10;
        m_zoomNote->setText(at == 100 ? tr("%1 % — la taille d'origine").arg(at)
                                      : tr("%1 %").arg(at));
    };
    connect(m_zoom, &QSlider::valueChanged, this, sayZoom);
    sayZoom();
    zoomRow->addWidget(m_zoom);
    zoomRow->addWidget(m_zoomNote);
    zoomRow->addStretch();
    rows->addLayout(zoomRow);
    rows->addSpacing(4);
    rows->addWidget(asideLabel(
        page, tr("Un jeu installé apporte des images d'une finesse donnée : agrandies, "
                 "les figures posées sur le tapis se lisent. Ctrl et la molette font la "
                 "même chose pendant la partie.")));
    rows->addStretch(1);
}

void SettingsDialog::buildFeltPage() {
    QVBoxLayout *rows = addPage(tr("Le tapis"), tr("Le tapis"));
    QWidget *page = m_pages->widget(m_pages->count() - 1);

    auto *feltRow = new QHBoxLayout;
    feltRow->setSpacing(8);
    for (const FeltColour &felt : feltColours()) {
        auto *swatch = new QToolButton(page);
        swatch->setCursor(Qt::PointingHandCursor);
        swatch->setFocusPolicy(Qt::NoFocus);
        swatch->setToolTip(felt.name);
        swatch->setFixedSize(58, 40);
        const int hue = felt.hue;
        connect(swatch, &QToolButton::clicked, this, [this, hue] {
            if (m_settings.hue == hue) return;
            m_settings.hue = hue;
            // Essayée en vrai: la boîte est elle-même peinte aux couleurs du
            // tapis, alors changer la teinte se voit tout de suite plutôt
            // qu'une fois la boîte refermée.
            setCurrentPalette(hue);
            refreshDialogChrome(this);
            // Les titres des quatre pages portent la couleur du tapis, et ils
            // la prennent au moment où ils sont faits: la feuille de style de
            // la boîte ne les atteint pas, il faut le leur dire.
            recolourSectionHeadings(this);
            dressFelts();
            dressLists();
            // L'ancre reprend la couleur du tapis, qui vient de changer.
            dressEditorLinks();
            // Les vignettes des dos sont dessinées aux couleurs du tapis: le
            // treillis que le programme dessine en est fait, le fond sur
            // lequel une carte transparente est posée aussi, et le papier des
            // figures dessinées avec son encre.
            fillBacks();
            if (m_faces) m_faces->update();
        });
        m_felts.append(swatch);
        feltRow->addWidget(swatch);
    }
    feltRow->addStretch();
    rows->addLayout(feltRow);
    rows->addSpacing(6);
    rows->addWidget(asideLabel(
        page, tr("La couleur du drap, et de tout ce qui va avec : la bande du haut, les "
                 "plaques, et les boîtes comme celle-ci. Elle ne dépend pas du nombre de "
                 "joueurs -- une table de six n'est pas d'une autre couleur qu'une table "
                 "de deux, elle est seulement plus pleine -- et chaque profil garde la "
                 "sienne.")));
    rows->addStretch(1);

    dressFelts();
}

void SettingsDialog::dressFelts() {
    for (int i = 0; i < m_felts.size() && i < feltColours().size(); ++i) {
        const FeltColour &felt = feltColours().at(i);
        // Chaque pastille porte le drap qu'elle propose, pas une case de
        // couleur: c'est le tapis qu'on choisit, avec sa bordure et son ombre,
        // et c'est lui qu'il faut montrer.
        const Palette scheme = paletteForHue(felt.hue, darkMode());
        const bool mine = felt.hue == m_settings.hue;
        m_felts.at(i)->setStyleSheet(
            QStringLiteral("QToolButton { border-radius: 8px; background: %1;"
                           " border: %2px solid %3; }"
                           "QToolButton:hover { border: 2px solid %4; }")
                .arg(scheme.felt.name())
                .arg(mine ? 3 : 1)
                .arg(mine ? currentPalette().accent.name() : scheme.feltEdge.name(),
                     currentPalette().accent.name()));
    }
}

void SettingsDialog::dressLists() {
    const Palette &scheme = currentPalette();

    // La liste des pages: la page en cours dans la couleur du tapis, les
    // autres à plat sur le corps de la boîte. Pas de cadre autour -- elle est
    // une colonne de la boîte et non un champ posé dedans -- mais un filet sur
    // sa droite, qui est ce qui sépare la liste de la page.
    if (m_sections) {
        m_sections->setStyleSheet(
            QStringLiteral("QTreeWidget { background: transparent; color: %1;"
                           " border: none; border-right: 1px solid %2; padding: 2px 6px 2px 0; }"
                           "QTreeView::item { border: 1px solid transparent;"
                           " border-radius: 6px; padding: 3px 8px 5px 8px; }"
                           "QTreeView::item:hover { background: %3; }"
                           "QTreeView::item:selected { background: %4; color: %5;"
                           " font-weight: bold; }")
                .arg(scheme.surfaceText.name(), scheme.edge.name(),
                     blendColours(scheme.surface, scheme.accent, 0.12).name(),
                     scheme.accent.name(), scheme.accentText.name()));
    }

    if (m_backs) {
        m_backs->setStyleSheet(
            QStringLiteral("QListWidget { background: %1; color: %2; border: 1px solid %3;"
                           " border-radius: 8px; padding: 3px; }"
                           "QListWidget::item { border: 1px solid transparent;"
                           " border-radius: 8px; padding: 4px; }"
                           "QListWidget::item:hover { border: 1px solid %4; }"
                           "QListWidget::item:selected { background: %5;"
                           " border: 1px solid %6; color: %2; }")
                .arg(scheme.field.name(), scheme.surfaceText.name(), scheme.edge.name(),
                     blendColours(scheme.edge, scheme.accent, 0.5).name(),
                     blendColours(scheme.field, scheme.accent, 0.18).name(),
                     scheme.accent.name()));
    }
}

void SettingsDialog::fillDecks() {
    CardDecks &decks = CardDecks::instance();
    const QString chosen = decks.chosenId();

    QSignalBlocker quiet(m_deck);
    m_deck->clear();
    for (const DeckEntry &entry : decks.entries()) {
        QString label = entry.name;
        if (!entry.builtIn && !entry.complete) {
            label += tr(" — %1 cartes sur %2").arg(entry.faces).arg(CardSet::kSlots);
        }
        m_deck->addItem(label, entry.id);
        if (entry.id == chosen) m_deck->setCurrentIndex(m_deck->count() - 1);
    }
    fillBackDecks();
    fillBacks();
    describeDeck();
}

void SettingsDialog::fillBackDecks() {
    if (!m_backDeck) return;
    const QString chosen = backDeckId();

    QSignalBlocker quiet(m_backDeck);
    m_backDeck->clear();
    // En tête, et choisi quand rien n'est écrit: le dos suit les faces.
    m_backDeck->addItem(tr("Celui du jeu de cartes"), QString());
    for (const DeckEntry &entry : CardDecks::instance().entries()) {
        m_backDeck->addItem(entry.name, entry.id);
        if (!chosen.isEmpty() && entry.id == chosen) {
            m_backDeck->setCurrentIndex(m_backDeck->count() - 1);
        }
    }
}

void SettingsDialog::fillBacks() {
    if (!m_backs) return;
    // Celui d'où le dos est tiré, qui n'est pas forcément celui des faces.
    const CardSet *set = backCards();

    // Muet pendant qu'on le remplit: sans cela, la première vignette posée se
    // ferait choisir et écrirait son numéro par-dessus celui qui est gardé.
    QSignalBlocker quiet(m_backs);
    m_backs->clear();

    // La même règle que le tapis: la vignette est exactement la carte que la
    // table dessinera, proportion et largeur comprises.
    const QSizeF box = cardSizeForHeight(kBackHeight);

    if (!set || set->backCount() == 0) {
        // Un seul aperçu, celui que le programme dessine -- et rien à choisir.
        auto *item =
            new QListWidgetItem(QIcon(backPreview(set, -1, box)), tr("celui du jeu"), m_backs);
        item->setData(Qt::UserRole, -1);
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        item->setFlags(Qt::ItemIsEnabled);
        return;
    }

    const int chosen = backIndex();
    for (int at = 0; at < set->backCount(); ++at) {
        auto *item =
            new QListWidgetItem(QIcon(backPreview(set, at, box)), set->backName(at), m_backs);
        item->setData(Qt::UserRole, at);
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        item->setToolTip(set->backName(at));
        if (at == chosen) m_backs->setCurrentItem(item);
    }
}

void SettingsDialog::describeDeck() {
    CardDecks &decks = CardDecks::instance();
    const DeckEntry *entry = decks.entry(decks.chosenId());
    if (!entry) {
        m_deckNote->setText(QString());
        return;
    }
    QString says;
    if (entry->builtIn) {
        says = tr("Les cartes que le jeu dessine lui-même.");
    } else {
        says = entry->note.trimmed();
        if (!entry->author.trimmed().isEmpty()) {
            says = tr("De %1. %2").arg(entry->author.trimmed(), says);
        }
        if (!entry->complete) {
            says += tr(" %1 cartes manquantes, dessinées par le jeu.")
                        .arg(CardSet::kSlots - entry->faces);
        }
    }
    m_deckNote->setText(says.trimmed());
}

GameSettings SettingsDialog::settings() const {
    // La longueur de la partie se choisit au menu d'ouverture, au-dessus des
    // tables dont elle change le compte: ce qui est ici vaut pour toutes.
    GameSettings chosen = m_settings;
    chosen.hue = m_settings.hue;
    chosen.pace = qBound(0, m_paces->checkedId(), 2);
    chosen.autoSort = m_autoSort->isChecked();
    chosen.offerHints = m_offerHints->isChecked();
    chosen.celebrate = m_celebrate->isChecked();
    chosen.zoom = int(std::round(m_zoom->value() / 10.0)) * 10;
    return chosen;
}
