#pragma once

#include <QPoint>
#include <QString>
#include <QWidget>

class QTimer;

// Carte d'un adversaire : son médaillon, son nom, son niveau, sa jauge de
// progression et ses résultats sous le profil courant.
//
// Elle est affichée dans le menu d'ouverture, où les six adversaires sont
// présentés avant le choix de la table : un clic invite l'adversaire, un clic
// droit demande l'ouverture du menu contextuel. La carte se borne à émettre
// cette demande, le menu étant construit par la page, seule à connaître les
// opérations applicables à un adversaire.
class RivalCard : public QWidget {
    Q_OBJECT

public:
    explicit RivalCard(const QString &rivalId, QWidget *parent = nullptr);

    QString rivalId() const { return m_rivalId; }

    void setInvited(bool invited);
    bool invited() const { return m_invited; }

    // Dans le menu, un clic invite l'adversaire ; ailleurs, la carte est
    // purement informative.
    void setPickable(bool pickable);
    // Affichage sur le tapis plutôt que sur le fond clair d'une fiche : la
    // carte est alors sombre et bordée d'or, et se limite aux éléments
    // essentiels — médaillon, nom, niveau et jauge. Les éléments omis — la
    // phrase descriptive et le palmarès — sont présentés par l'aperçu au
    // survol et par la fiche, qui disposent de la place nécessaire.
    void setOnFelt(bool onFelt);

    // Le survol fait apparaître l'aperçu de ses huit compétences. La carte
    // figurant dans la fiche n'active pas ce comportement, étant déjà placée
    // au-dessus des huit lignes que l'aperçu résume.
    void setGlanceWanted(bool wanted);

    // À appeler après une modification de l'expérience ou un changement de
    // profil.
    void reread();

    QSize sizeHint() const override;

Q_SIGNALS:
    void invitedChanged();
    void menuWanted(const QPoint &where);
    // Double-clic : ouverture de la fiche. La carte ne la construit pas ;
    // c'est la page qui l'ouvre, comme pour le menu contextuel.
    void sheetWanted();

protected:
    void paintEvent(QPaintEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QString m_rivalId;
    // Minuterie de survol : l'aperçu n'apparaît qu'après un maintien du
    // pointeur sur la carte.
    QTimer *m_glanceBeat = nullptr;
    bool m_invited = false;
    bool m_onFelt = false;
    bool m_pickable = false;
    bool m_hovered = false;
    bool m_glanceWanted = true;
};
