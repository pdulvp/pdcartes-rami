# Plans de travail

Le plan en cours vit ici, dans `plan.md`, avec un en-tête
`status: in-progress | complete`. Ce fichier est ignoré par git: c'est un
brouillon de session, pas de l'historique. Il est dans le dépôt pour survivre
à un plantage *et* à un redémarrage (`/tmp` est vidé au reboot).

Quand il passe à `complete` — ou qu'il va être remplacé par un nouveau plan —
il est archivé à côté sous `AAAA-MM-JJ-HHMM-slug.md`, l'horodatage étant celui
de la fin des travaux (champ `updated` de l'en-tête), puis `plan.md` est
supprimé. Le préfixe de date suffit au classement chronologique; pas de
numérotation à faire collisionner.

Les fichiers datés sont des archives: on les lit, on ne les modifie plus.

Tout ce dossier est ignoré par git, ce `README.md` excepté. L'historique des
travaux ne vit donc pas ici: il vit dans les tickets GitHub, ouverts au moment
du commit.

## Du plan au ticket

Les crochets de prel (`core.hooksPath`) classent les plans terminés au premier
commit qui suit. Au moment où un plan passe à `complete`, son résumé est écrit
dans `sumup.md`, dans le format que décrit la compétence `plan-summary` de
prel. Au commit suivant:

- un ticket GitHub est ouvert, reprenant le résumé puis le texte intégral de
  chaque plan `complete` pas encore classé — **un ticket pour le lot**, un plan
  et un commit ne se correspondant pas un pour un;
- le sujet du commit commence par le numéro du ticket:
  `#12 Paquetage Windows avec les DLL de Qt`;
- chaque plan classé reçoit un champ `issue: 12` dans son en-tête, et
  `sumup.md` est supprimé: son contenu vit dans le ticket désormais.

Sans `sumup.md`, rien n'est classé et le commit est un commit ordinaire. Pour
voir ce que le prochain commit classerait:

    prel issue-from-plans status
    prel issue-from-plans dry-run
