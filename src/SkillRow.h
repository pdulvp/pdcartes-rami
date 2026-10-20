#pragma once

#include <QString>
#include <QWidget>

// La ligne d'une compétence, dans la fiche d'un adversaire.
//
// Huit lignes de la même forme, quatre et quatre: son nom, ses pastilles, le
// mot du niveau où il en est, et ce que ce niveau change à la table. Les
// acquises ont en plus leur rang en chiffres romains et la jauge de ce qui
// manque pour le suivant.
//
// Chaque compétence a sa couleur, et ce n'est pas celle du tapis: le jeu
// entier descend d'une seule teinte -- jade à deux, bordeaux à six -- et une
// fiche de huit lignes toutes vertes serait une fiche qu'on ne lit pas. Les
// quatre chaudes sont ce qu'il est, les quatre froides ce qu'il a appris; la
// couleur seule dit quelle moitié on regarde.
//
// Une ligne innée peut se changer: ses pastilles se cliquent, et c'est la
// fiche qui sait ce que cela coûte.
//
// Serrée, elle ne garde que ses trois premières colonnes -- le nom, les
// pastilles, le rang et son mot -- et se tasse: c'est la ligne de l'aperçu
// qui flotte au survol d'un adversaire, où il y a huit lignes à montrer sur
// deux colonnes et pas la place d'une phrase. Voir RivalGlance.h.
class SkillRow : public QWidget {
    Q_OBJECT

public:
    explicit SkillRow(QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setHue(int hue);
    // Le niveau où il en est, et combien il y en a.
    void setLevel(int level, int outOf);
    void setWord(const QString &word);
    void setSays(const QString &says);
    // Les acquises seulement: le rang en romain, et la jauge du rang suivant.
    void setRank(const QString &roman);
    void setGauge(qreal filled, const QString &right);

    // Les pastilles se cliquent.
    void setEditable(bool editable);
    // Éteinte: ce qui va s'effacer, pendant qu'on change ce qu'il est.
    void setDim(bool dim);
    // Serrée: les trois premières colonnes seules, et la ligne basse.
    void setCompact(bool compact);

    int level() const { return m_level; }

    QSize sizeHint() const override;

Q_SIGNALS:
    void levelChosen(int level);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    // Quelle pastille se trouve sous ce point, ou -1.
    int pipAt(const QPointF &where) const;
    QRectF pipBox(int index) const;
    // Les colonnes, qui ne sont pas les mêmes serrée ou non.
    qreal titleWide() const;
    qreal romanWide() const;
    qreal wordWide() const;
    qreal rowHigh() const;

    QString m_title;
    QString m_word;
    QString m_says;
    QString m_roman;
    QString m_right;
    int m_hue = 200;
    int m_level = 0;
    int m_outOf = 4;
    qreal m_filled = -1.0;      // négatif: pas de jauge
    bool m_editable = false;
    bool m_dim = false;
    bool m_compact = false;
    int m_hovered = -1;
};
