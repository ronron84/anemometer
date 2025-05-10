Bien sûr ! Voici un exemple de page explicative que vous pouvez utiliser pour votre dépôt GitHub. Cette page explique comment installer et utiliser votre projet de station météo sur une carte RP2040.

---

# Station Météo RP2040

Ce projet est une station météo basée sur une carte RP2040 qui mesure la vitesse et la direction du vent, la température, et envoie des SMS avec les données météo.

## Fonctionnalités

- Mesure de la vitesse et de la direction du vent.
- Mesure de la température.
- Envoi de SMS avec les données météo.
- Configuration via une interface série.

## Prérequis

- Carte RP2040
- Capteur de température DS18B20
- Capteur de vitesse et de direction du vent
- Module SIM pour l'envoi de SMS
- Arduino IDE

## Installation

1. **Cloner le dépôt** :
   ```bash
   git clone https://github.com/votre-utilisateur/station-meteo-rp2040.git
   ```

2. **Ouvrir le projet dans Arduino IDE** :
   - Ouvrez l'Arduino IDE.
   - Allez dans `Fichier > Ouvrir` et sélectionnez le fichier `.ino` du projet.

3. **Configurer l'Arduino IDE** :
   - Sélectionnez la carte RP2040 dans `Outils > Carte`.
   - Sélectionnez le port série approprié dans `Outils > Port`.

4. **Télécharger le code sur la carte RP2040** :
   - Cliquez sur le bouton `Télécharger` pour compiler et télécharger le code sur la carte RP2040.

## Utilisation

1. **Configurer les paramètres** :
   - Utilisez l'interface série pour configurer les paramètres de la station météo.
   - Envoyez des commandes série pour définir les coefficients, les offsets, le numéro de téléphone, etc.

2. **Envoyer des SMS** :
   - Utilisez la commande `SENDSMS` pour envoyer un SMS avec les données météo actuelles.

3. **Sauvegarder la configuration** :
   - Utilisez la commande `SAVE` pour sauvegarder la configuration actuelle dans l'EEPROM.

## Commandes Série

- `?` : Affiche l'aide des commandes disponibles.
- `LIST` : Affiche les valeurs actuelles.
- `AC=<val>` : Définit le coefficient de l'anémomètre.
- `AO=<val>` : Définit l'offset de l'anémomètre.
- `GC=<val>` : Définit le coefficient de la girouette.
- `GO=<val>` : Définit l'offset de la girouette.
- `TO=<val>` : Définit l'offset de la température.
- `PN=<val>` : Définit le numéro de téléphone.
- `GF=<val>` : Définit le facteur de rafale.
- `GD=<val>` : Définit la différence minimale de rafale.
- `GT=<val>` : Définit la durée minimale de rafale.
- `SAVE` : Sauvegarde la configuration dans l'EEPROM.
- `SIMNUM` : Affiche le numéro de la carte SIM.
- `SENDSMS` : Envoie un SMS de test.

## Exemple de Commande

Pour définir le coefficient de l'anémomètre à 1.5 :
```
AC=1.5
```

Pour sauvegarder la configuration :
```
SAVE
```
![20250331_072737](https://github.com/user-attachments/assets/3cccc44e-6596-45ef-9879-ac85acc09ba2)
## Exemple config de base pour les rafales 
Valeurs par défaut pour les rafales
Facteur de Rafale (GUST_FACTOR) :

Valeur par défaut : 1.5
Description : Ce facteur est utilisé pour déterminer si une augmentation de la vitesse du vent est considérée comme une rafale. Une valeur de 1.5 signifie que la vitesse du vent doit être 1,5 fois supérieure à la vitesse moyenne pour être considérée comme une rafale.
Différence Minimale de Rafale (MIN_GUST_DIFF) :

Valeur par défaut : 5.0
Description : Cette valeur représente la différence minimale absolue en km/h entre la vitesse actuelle du vent et la vitesse moyenne pour qu'une augmentation soit considérée comme une rafale.
Durée Minimale de Rafale (GUST_MIN_DURATION) :

Valeur par défaut : 120000 (2 minutes en millisecondes)
Description : Cette valeur représente la durée minimale pendant laquelle la vitesse du vent doit rester élevée pour être considérée comme une rafale.
## Contribution

Les contributions sont les bienvenues ! Veuillez ouvrir une issue ou soumettre une pull request pour toute amélioration ou correction.

## Licence

Ce projet est sous licence MIT. Voir le fichier `LICENSE` pour plus de détails.

---

