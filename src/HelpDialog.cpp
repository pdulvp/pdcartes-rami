#include "HelpDialog.h"

#include "DialogChrome.h"
#include "Palette.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace {

// Feuille de style du document. Les couleurs sont celles de la palette
// courante, comme dans le reste de l'application : la fenêtre des règles
// ouverte depuis une table bordeaux s'affiche en bordeaux.
QString sheetFor(const Palette &scheme) {
    return QStringLiteral(R"CSS(
        h1 { color: %1; font-size: 17px; margin-top: 22px; margin-bottom: 2px; }
        h2 { color: %1; font-size: 13px; margin-top: 16px; margin-bottom: 2px; }
        p { color: %2; margin-top: 6px; margin-bottom: 6px; }
        p.lead { color: %2; font-size: 13px; }
        li { color: %2; margin-bottom: 3px; }
        b { color: %1; }
        th { color: %3; text-align: left; padding-right: 14px;
             padding-top: 3px; padding-bottom: 3px; font-weight: bold; }
        td { color: %2; padding-top: 3px; padding-bottom: 3px; }
        kbd { color: %1; font-weight: bold; }
    )CSS")
        .arg(scheme.accent.name(), scheme.surfaceText.name(), scheme.hint.name());
}

// Texte intégral des règles. Il est inscrit dans le source plutôt que placé
// dans un fichier distinct, afin qu'aucun fichier ne soit à installer auprès
// de l'exécutable ni à rechercher à l'exécution.
const char *kRules = R"HTML(
<h1>Le rami</h1>

<p class="lead">Le rami se joue avec deux jeux de cinquante-deux cartes
mélangés ensemble et quatre jokers par-dessus : cent huit cartes. Chacun en
reçoit quatorze. Le but est de se débarrasser de toutes les siennes en les
rangeant en combinaisons, avant les autres.</p>

<h2>Les combinaisons</h2>

<p>Il n'en existe que deux.</p>

<table>
<tr><th>Une suite</th><td>Trois cartes ou plus de la même couleur qui se
suivent : <b>7-8-9 de cœur</b>. L'as se met sous le deux ou au-dessus du roi,
mais jamais les deux à la fois : <b>A-2-3</b> et <b>D-R-A</b> sont des suites,
<b>R-A-2</b> n'en est pas une.</td></tr>
<tr><th>Une série</th><td>Trois ou quatre cartes du même rang, dans des
couleurs différentes : <b>trois dames</b>, ou quatre. Les deux jeux donnent
deux dames de cœur, et deux dames de cœur ne font pas une série — c'est deux
fois la même couleur.</td></tr>
</table>

<h2>Ce que valent les cartes</h2>

<table>
<tr><th>L'as</th><td>11 points, sauf dans la suite A-2-3 où il vaut 1</td></tr>
<tr><th>Roi, dame, valet</th><td>10 points</td></tr>
<tr><th>Du deux au dix</th><td>ce qui est écrit dessus</td></tr>
<tr><th>Le joker</th><td>dans une combinaison, la valeur de la carte qu'il
remplace ; <b>resté en main, 25 points</b>, ce qui dissuade de le
conserver</td></tr>
</table>

<h2>Un tour</h2>

<ol>
<li><b>Piocher</b> une carte : celle du dessus de la pioche, ou celle qui est
retournée sur la défausse. Une seule.</li>
<li><b>Poser</b> autant de combinaisons qu'on veut — mais la première fois,
voir plus bas.</li>
<li><b>Compléter</b> le tapis : une carte au bout d'une suite, une quatrième
couleur sur une série. N'importe quelle combinaison posée, la vôtre ou celle
de n'importe qui.</li>
<li><b>Défausser</b> une carte, face en haut. C'est ce qui finit le tour.</li>
</ol>

<h2>Ouvrir : cinquante-et-un points</h2>

<p>La première pose doit valoir <b>51 points au moins</b>, atteints dans un
seul tour. Tant que vous n'avez pas ouvert, vous ne pouvez ni poser ni
compléter : vous piochez et vous défaussez jusqu'à réunir ce total.</p>

<p>Une pose se construit combinaison par combinaison ; le total en cours est
affiché sous votre main. Dès qu'il atteint 51, la pose est placée sur le tapis
et vous êtes ouvert pour le reste de la manche, avec le droit de compléter
durant le même tour. Si le tour s'achève avant que ce total soit atteint, vous
devez <b>reprendre la pose en main</b> : il n'est pas permis de défausser en
laissant une pose incomplète.</p>

<p>Tant qu'elle n'est pas placée, une pose se défait carte par carte :
<b>cliquez une carte de la pose</b> et elle revient seule en main, à condition
que sa combinaison reste valide sans elle — une suite perd alors une
extrémité, une série de quatre perd sa quatrième carte. Si la combinaison
n'est plus valide, elle revient en main en entier. Le bouton
<b>Reprendre</b> rend la pose complète en une fois.</p>

<h2>Le joker</h2>

<p>Un joker remplace n'importe quelle carte, à raison d'un seul par
combinaison. La carte qu'il représente est fixée au moment où la combinaison
est posée et ne change plus : dans <b>5-★-7 de carreau</b>, le joker est le
six de carreau.</p>

<p>Un joker posé peut être <b>racheté</b>. Si vous avez ouvert et que vous
détenez exactement la carte qu'il représente, vous échangez l'une contre
l'autre. Le joker doit alors être réemployé dans le tour même : vous devez le
poser dans une nouvelle combinaison ou l'ajouter à une combinaison existante
avant de défausser. Dans une série, un joker peut représenter n'importe
laquelle des couleurs absentes.</p>

<h2>Sortir, et compter</h2>

<p>La manche s'achève dès qu'un joueur n'a plus de cartes : il a posé ou
complété toutes les siennes, puis défaussé la dernière. Il marque
<b>zéro</b>. Les autres comptent les points des cartes restées en main. Une
pose commencée qui n'a jamais atteint 51 points est comptée avec elles :
c'est le risque que l'on prend en en commençant une.</p>

<p>Les points sont des pénalités : ils s'additionnent d'une manche à
l'autre, et <b>le joueur qui en totalise le moins remporte la partie</b>.
Celle-ci s'arrête dès qu'un joueur dépasse le total fixé.</p>

<p>Si la pioche s'épuise, la défausse est retournée pour la reconstituer,
sa carte supérieure restant en place. Après deux reconstitutions, la manche
est interrompue et chaque joueur compte sa main.</p>

<h1>À la souris</h1>

<table>
<tr><th>Clic sur la pioche ou la défausse</th><td>piocher — c'est la première
chose à faire de chaque tour</td></tr>
<tr><th>Clic sur une carte de votre main</th><td>la choisir, ou la laisser ;
les cartes choisies se soulèvent</td></tr>
<tr><th><b>Poser</b></th><td>pose les cartes choisies en une combinaison</td></tr>
<tr><th><b>Compléter</b></th><td>met chaque carte choisie sur la première
combinaison du tapis qui la prend</td></tr>
<tr><th>Clic sur une combinaison du tapis</th><td>y met les cartes choisies</td></tr>
<tr><th>Clic droit sur une combinaison</th><td>rachète son joker avec la carte
choisie</td></tr>
<tr><th>Clic sur une carte de votre pose</th><td>la reprend en main — ou toute
sa combinaison, si elle ne tient plus sans elle</td></tr>
<tr><th><b>Reprendre</b></th><td>rend toute la pose en cours à votre main</td></tr>
<tr><th><b>Défausser</b></th><td>jette la carte choisie et finit le tour</td></tr>
<tr><th>Double-clic sur une carte</th><td>la défausse directement</td></tr>
<tr><th><b>Conseil</b></th><td>indique le coup que jouerait le meilleur
joueur de la table : la même logique que celle d'Oriane, sans ses erreurs
délibérées</td></tr>
</table>

<h2>Au clavier</h2>

<table>
<tr><th><kbd>Espace</kbd></th><td>piocher</td></tr>
<tr><th><kbd>Entrée</kbd></th><td>poser</td></tr>
<tr><th><kbd>C</kbd></th><td>compléter</td></tr>
<tr><th><kbd>D</kbd></th><td>défausser</td></tr>
<tr><th><kbd>T</kbd> / <kbd>R</kbd></th><td>ranger la main par couleur / par rang</td></tr>
<tr><th><kbd>H</kbd></th><td>un conseil</td></tr>
<tr><th><kbd>F5</kbd></th><td>une autre partie à la même table</td></tr>
<tr><th><kbd>Échap</kbd></th><td>revenir au menu</td></tr>
</table>

<h1>Les six autres</h1>

<p>Les six adversaires ne sont pas six niveaux de difficulté d'un même
joueur : chacun possède sa propre progression. Deux débutent en ne connaissant
presque rien du jeu, trois le maîtrisent, et Oriane le pratique de longue
date.</p>

<p>Leur niveau tient en une seule valeur, l'<b>expérience</b>. Elle augmente
à chaque manche jouée, et davantage à chaque manche gagnée. Sa progression
ralentit à mesure qu'elle s'élève : les premiers paliers sont atteints en
quelques parties, les derniers demandent beaucoup plus de temps. Elle est
conservée d'une session à l'autre, de sorte que les adversaires continuent de
progresser au fil des parties.</p>

<table>
<tr><th>Débutant</th><td>0 à 199 — analyse sa main de façon incomplète et
laisse passer des ouvertures possibles, se défausse de cartes qui lui
auraient servi, ne prend la défausse que lorsqu'elle complète immédiatement
une combinaison, et commet des erreurs manifestes</td></tr>
<tr><th>Intermédiaire</th><td>200 à 599 — analyse sa main exactement et
n'en laisse échapper aucune combinaison, évalue l'intérêt de la défausse,
conserve les cartes susceptibles de servir, et évite de défausser une carte
immédiatement jouable par un adversaire</td></tr>
<tr><th>Expert</th><td>600 à 1200 — mémorise toutes les cartes passées,
sait combien d'exemplaires de chaque carte restent en jeu, tient compte des
cartes que vous ramassez pour ne pas vous les redonner, rachète ses jokers,
et ne commet plus d'erreur</td></tr>
</table>

<h2>Qui est à table</h2>

<p>Les places libres sont attribuées aux adversaires <b>les moins
sollicités</b>, afin que les six progressent à un rythme comparable. Dans le
menu, un clic sur la carte d'un adversaire l'<b>invite</b> : les invités sont
placés en premier, les places restantes étant attribuées à tour de rôle.</p>

<h2>Les trois profils</h2>

<p>L'expérience des six adversaires est enregistrée dans le profil sous
lequel vous jouez, et dans celui-là seul. Changer de profil revient donc à
changer de table : l'adversaire affronté quarante fois sur le premier profil
est distinct de son homonyme sur le deuxième. Dans les réglages, chaque
adversaire dispose d'un bouton qui le ramène à son niveau initial, pour lui
seul et sur le profil courant.</p>
)HTML";

} // namespace

HelpDialog::HelpDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Les règles du rami"));

    const DialogChrome chrome = applyDialogChrome(this, tr("Les règles"));
    const Palette &scheme = currentPalette();

    m_pages = new QTextBrowser(this);
    m_pages->setOpenExternalLinks(false);
    m_pages->document()->setDefaultStyleSheet(sheetFor(scheme));
    m_pages->setHtml(QString::fromUtf8(kRules));
    m_pages->moveCursor(QTextCursor::Start);
    chrome.fields->addWidget(m_pages, 1);

    auto *buttons = new QDialogButtonBox(this);
    QPushButton *ok = buttons->addButton(tr("Entendu"), QDialogButtonBox::AcceptRole);
    markDefaultButton(ok);
    ok->setDefault(true);
    chrome.footer->addStretch();
    chrome.footer->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

    resize(660, 720);
}
