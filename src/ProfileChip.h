#pragma once

#include <QAbstractButton>
#include <QString>

// Plaque de profil affichée dans le bandeau supérieur.
//
// Elle indique le profil courant et ouvre un menu permettant d'en changer,
// de le renommer ou de l'effacer. Ces fonctions étaient auparavant réparties
// entre une section du menu d'ouverture et une ligne de texte du bandeau ;
// elles sont désormais réunies en un seul élément, visible pendant la partie
// comme avant celle-ci.
//
// La plaque est dessinée et non stylée par feuille de style : une pastille
// ronde portant l'initiale, le nom, et un chevron signalant la présence d'un
// menu. Un `QPushButton` stylé aurait eu l'apparence d'un bouton de
// formulaire au milieu d'un bandeau qui n'en comporte aucun.
class ProfileChip : public QAbstractButton {
    Q_OBJECT

public:
    explicit ProfileChip(QWidget *parent = nullptr);

    // Relit le profil courant, après un changement de nom ou de profil.
    void reread();

    // La bande qui la porte est sombre: la plaque prend l'encre claire du
    // drap et l'or, et non les couleurs pâles des panneaux. Demandé par la
    // fenêtre plutôt que deviné de la palette héritée -- une feuille de style
    // sur la bande fait redépolir tout ce qui est dedans, et rend à chacun la
    // palette qu'il avait avant elle.
    void setOnDark(bool onDark);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QFont nameFont() const;

    QString m_name;
    bool m_hovered = false;
    bool m_onDark = false;
};
