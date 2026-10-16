#include "Tables.h"

#include <QCoreApplication>

#include <algorithm>

namespace {

const QVector<TableKind> &builtTables() {
    static const QVector<TableKind> kinds = {
        {QStringLiteral("deux"), QCoreApplication::translate("tables", "Tête-à-tête"), 2, 168},
        {QStringLiteral("trois"), QCoreApplication::translate("tables", "À trois"), 3, 196},
        {QStringLiteral("quatre"), QCoreApplication::translate("tables", "À quatre"), 4, 142},
        {QStringLiteral("cinq"), QCoreApplication::translate("tables", "À cinq"), 5, 32},
        {QStringLiteral("six"), QCoreApplication::translate("tables", "À six"), 6, 348},
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
    // The table played at by default, so that a settings file written by a
    // later version does not leave the game with no table at all.
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
    // Nine points a manche for each player who is not you: measured rather
    // than guessed, by playing a few thousand manches at every table and
    // watching how fast the worst score climbs. It is near enough linear in
    // the number of opponents, which is not a surprise -- every one of them
    // is another hand of cards that gets counted against somebody.
    return 9 * std::max(1, seats - 1) * target.manches;
}
