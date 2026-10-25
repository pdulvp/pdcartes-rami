#include "Tables.h"

#include <QCoreApplication>

#include <algorithm>

namespace {

const QVector<TableKind> &builtTables() {
    static const QVector<TableKind> kinds = {
        {QStringLiteral("deux"), QCoreApplication::translate("tables", "Tête-à-tête"), 2},
        {QStringLiteral("trois"), QCoreApplication::translate("tables", "À trois"), 3},
        {QStringLiteral("quatre"), QCoreApplication::translate("tables", "À quatre"), 4},
        {QStringLiteral("cinq"), QCoreApplication::translate("tables", "À cinq"), 5},
        {QStringLiteral("six"), QCoreApplication::translate("tables", "À six"), 6},
    };
    return kinds;
}

const QVector<Target> &builtTargets() {
    static const QVector<Target> list = {
        {QStringLiteral("manche"), QCoreApplication::translate("tables", "Une manche"),
         QCoreApplication::translate("tables", "Une seule donne, et c'est fini"), 1},
        {QStringLiteral("courte"), QCoreApplication::translate("tables", "Partie courte"),
         QCoreApplication::translate("tables", "Quatre manches environ"), 4},
        {QStringLiteral("normale"), QCoreApplication::translate("tables", "Partie normale"),
         QCoreApplication::translate("tables", "Huit manches environ"), 8},
        {QStringLiteral("longue"), QCoreApplication::translate("tables", "Partie longue"),
         QCoreApplication::translate("tables", "Une douzaine de manches"), 13},
    };
    return list;
}

} // namespace

const QVector<TableKind> &tableKinds() { return builtTables(); }

const TableKind &tableKindById(const QString &id) {
    for (const TableKind &kind : builtTables()) {
        if (kind.id == id) return kind;
    }
    // Table utilisée par défaut, afin qu'un fichier de réglages écrit par une
    // version ultérieure ne laisse pas l'application sans table.
    return builtTables().at(2);
}

QString defaultTableId() { return builtTables().at(2).id; }

const QVector<Target> &targets() { return builtTargets(); }

const Target &targetById(const QString &id) {
    for (const Target &target : builtTargets()) {
        if (target.id == id) return target;
    }
    return builtTargets().at(2);
}

QString defaultTargetId() { return builtTargets().at(2).id; }

int targetPointsFor(const Target &target, int seats) {
    if (target.manches <= 1) return 0;
    // Neuf points par manche et par adversaire. Cette valeur est mesurée et
    // non estimée : quelques milliers de manches ont été jouées à chaque
    // table en observant la vitesse de progression du plus mauvais score. La
    // relation est sensiblement linéaire en fonction du nombre d'adversaires,
    // ce qui s'explique : chacun d'eux représente une main supplémentaire dont
    // les points sont comptés à l'un des joueurs.
    return 9 * std::max(1, seats - 1) * target.manches;
}
