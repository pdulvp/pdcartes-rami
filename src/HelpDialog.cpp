#include "HelpDialog.h"

#include "DialogChrome.h"
#include "Palette.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace {

// La feuille de style du document. Les couleurs viennent de la table que le
// jeu porte, comme partout ailleurs : une boîte de règles ouverte sur une
// table bordeaux est bordeaux.
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

// Les règles, en entier. Écrites ici plutôt que dans un fichier à côté du
// binaire : un fichier à côté du binaire est un fichier qui manque sur la
// machine de quelqu'un d'autre.
const char *kRules = R"HTML(
<h1>Le rami</h1>

<p class="lead">Le rami se joue avec deux jeux de cinquante-deux cartes
mélangés ensemble et quatre jokers par-dessus : cent huit cartes. Chacun en
reçoit quatorze. Le but est de se débarrasser de toutes les siennes en les
rangeant en combinaisons, avant les autres.</p>

<h2>Les combinaisons</h2>

<p>Il n'y en a que deux, et il n'y en a jamais eu d'autres.</p>

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
remplace ; <b>resté en main, 25 points</b>, et c'est pour ça que personne ne le
garde</td></tr>
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

<p>La première pose doit valoir <b>51 points au moins</b>, et dans le même
tour. Tant qu'on n'a pas ouvert, on ne peut rien poser et rien compléter — on
pioche et on défausse, et on attend d'avoir de quoi.</p>

<p>Une pose peut se construire combinaison par combinaison : le compte en cours
est affiché sous votre main. Dès qu'il atteint 51 elle part sur le tapis et
vous êtes ouvert pour le reste de la manche, avec le droit de compléter dans le
même tour. Si le tour se termine et qu'elle n'y est pas, il faut la
<b>reprendre en main</b> : les règles ne laissent pas défausser avec une pose
qui ne compte pas.</p>

<h2>Le joker</h2>

<p>Un joker tient la place de n'importe quelle carte, et il n'en va pas plus
d'un par combinaison. La carte qu'il remplace est décidée au moment où la
combinaison est posée, et elle ne change plus : dans <b>5-★-7 de carreau</b> le
joker est le six de carreau, et rien d'autre.</p>

<p>On peut le <b>racheter</b>. Si vous avez ouvert et que vous tenez en main
exactement la carte dont il tient la place, vous la donnez et vous prenez le
joker — mais il doit servir tout de suite, dans la même tour : le posera ou le
mettra au bout d'une combinaison avant de défausser. Dans une série, le joker
remplace n'importe laquelle des couleurs qui manquent.</p>

<h2>Sortir, et compter</h2>

<p>La manche s'arrête quand quelqu'un n'a plus de cartes : il a posé ou
complété tout ce qu'il avait, et il a défaussé la dernière. Il marque
<b>zéro</b>. Tous les autres comptent ce qui leur reste en main — et une pose
commencée qui n'a jamais atteint 51 points compte avec le reste, ce qui est
tout le risque d'en commencer une.</p>

<p>Les points sont des pénalités : ils s'additionnent d'une manche à l'autre,
et <b>c'est celui qui en a le moins qui gagne</b> la partie. Elle s'arrête dès
que quelqu'un dépasse le but.</p>

<p>Si la pioche s'épuise, la défausse est retournée et redistribuée, en
laissant sa carte du dessus. Au bout de deux fois, la manche est arrêtée là et
tout le monde compte sa main.</p>

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
<tr><th><b>Défausser</b></th><td>jette la carte choisie et finit le tour</td></tr>
<tr><th>Double-clic sur une carte</th><td>la défausse directement</td></tr>
<tr><th><b>Conseil</b></th><td>dit ce que le meilleur joueur de la table ferait
de votre main — le même cerveau qu'Oriane, sans ses bourdes</td></tr>
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

<p>Les adversaires ne sont pas six réglages d'une même difficulté. Chacun est
un joueur avec un passé : deux s'assoient en ne sachant presque rien, trois
connaissent le jeu, et Oriane y joue depuis des années. Ce qu'ils savent est
un seul chiffre, leur <b>expérience</b>, qui monte un peu à chaque manche
jouée et un peu plus à chaque manche gagnée — et qui est toujours là quand
vous revenez. Une table que vous battiez le mois dernier est une table qui
vous bat ce mois-ci.</p>

<table>
<tr><th>Débutant</th><td>0 à 199 — lit sa main de travers et manque des
ouvertures qui étaient là, jette ce dont il avait besoin, ne prend la défausse
que quand elle finit une combinaison sous son nez, et se trompe franchement de
temps en temps</td></tr>
<tr><th>Intermédiaire</th><td>200 à 599 — lit sa main exactement et ne manque
plus rien de ce qu'elle contient, soupèse la défausse, garde ce qui peut
servir, et évite de vous donner ce qui se pose tout de suite</td></tr>
<tr><th>Expert</th><td>600 à 1200 — se souvient de toutes les cartes passées,
sait combien d'exemplaires d'une carte sont encore dehors, remarque ce que
vous ramassez et ne vous le redonne pas, rachète ses jokers, et ne fait plus
de bourde</td></tr>
</table>

<h2>Qui est à table</h2>

<p>Les places libres vont aux <b>moins joués</b>, pour que les six progressent
à peu près ensemble au lieu de laisser les trois derniers débutants pour
toujours. Au menu, un clic sur une carte d'adversaire l'<b>invite</b> : les
invités s'assoient d'abord, le reste des places au tour de rôle.</p>

<h2>Les trois profils</h2>

<p>L'expérience des six est comptée dans le profil sous lequel vous jouez, et
nulle part ailleurs. Changer de profil, c'est changer de table : le Lulu que
vous avez battu quarante fois sur le premier profil n'est pas celui du
deuxième. Et dans les réglages, chacun des six porte un bouton qui le remet au
niveau avec lequel il est arrivé, lui seul et sur ce profil seulement.</p>
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
