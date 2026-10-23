#pragma once

#include <QAbstractButton>
#include <QString>

// La plaque du profil, dans la bande du haut.
//
// Elle dit sous quel nom on joue et elle s'ouvre: le profil se change d'ici,
// se renomme d'ici, s'oublie d'ici. C'était une section du menu d'ouverture
// et une ligne de texte là-haut; c'est maintenant une seule chose, visible
// pendant la partie comme avant elle.
//
// Dessinée plutôt que stylée: une pastille ronde avec l'initiale, le nom, et
// le petit chevron qui dit qu'il y a quelque chose dessous. Un QPushButton
// sous feuille de style aurait eu l'air d'un bouton de formulaire au milieu
// d'une bande qui n'en porte aucun.
class ProfileChip : public QAbstractButton {
    Q_OBJECT

public:
    explicit ProfileChip(QWidget *parent = nullptr);

    // Relit le profil courant: son nom a changé, ou ce n'est plus le même.
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
