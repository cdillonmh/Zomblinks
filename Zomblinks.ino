/*    To Dos:
 *        ~~Necessary~~
 *     - Create start & reset code that propogates to everything
 *        + From spawners? Maybe triple-click?
 *      
 *        ~~Nice to Have~~
 *     - Animation & effects
 *     - Array and queue for projectile passage through single blink? (rare, but possible and annoying)
 *     
 *        ~~Maybe~~
 *     - Ammo limits and ammo drops
 *        + move to reload?
 *        + press to reload (rather than switch?)
 *        + zombies drop ammo?
 */

// Game mechanics variables
#define TANKMOVETIMEOUTMS 100
#define DAMAGEDELAYMS 60

// Game balance variables
#define WALLHEALTH 6
#define WALLREGENMS 2500
#define BULLETDELAY 50
#define ROCKETDELAY 250
#define STARTGAMEDELAY 6000
#define MELEEDAMAGE 2

// Zombie variables
#define SHAMBLEDELAY 1200
#define HUMANSCENT 16
#define BITEDELAYMS 550
#define MINHUNGER 2
#define MAXHUNGER 4
#define SPAWNTRIGGER 1
#define SPAWNMAX 500

// Color defaults
#define TANKCOLOR CYAN
#define TANKTWOCOLOR YELLOW
#define TANKONECOLOR RED
#define WALLCOLOR makeColorRGB(200, 200, 200)
#define WALLOFFCOLOR ORANGE
#define BULLETCOLOR WHITE
#define ROCKETCOLOR BLUE
#define MINECOLOR RED
#define EMPTYCOLOR OFF
#define BOOMCOLOR YELLOW
#define ZOMBIECOLOR GREEN

// Objects contained in a Blink
enum objectStates {NOTHING, HASWALL, HASTANK, REQTANK, GIVETANK3, GIVETANK2, GIVETANK1, HASBOOM};
byte objectState = NOTHING;

Timer tankTimeoutTimer;
bool waitingToTransfer;

// Health for walls and tanks
int objectHealth = -1;

int tankDirection = -1;
int tankSource = -1;

// States for passing projectiles and for loaded projectile in tank
enum projectileStates {NONE, HASBULLET, SENDBULLET, RCVBULLET, HASROCKET, SENDROCKET, RCVROCKET, ISUNDEAD};
byte projectileState = NONE;

byte tempProjectileStorage = NONE;
Timer damageDelayTimer;

int projectileSource = -1;
int projectileTarget = -1;
Timer projectileTimer;
//int ammo = 6;

Timer biteTimer;
bool attacking = false;
int biteTarget = -1;

int zombieWalk = SHAMBLEDELAY;

Timer startGameTimer;
Timer wallRegenTimer;
bool wasWallDamaged = false;
bool wallSpawning = false;

/*  COMM LAYOUT
 *   
 *        32          16          8           4          2          1
 *      <-----projectile_states----->       <------object_states------>
 *
 */

/*  REQUEST TANK PROCEDURE
 *   
 *          Target          |          Source
 *    1. Check for 1 tank   | 1. Only tank adjacent
 *    2. REQ from Source    | 2. Ack REQ with GIVETANK# to target
 *    3. Ack give with HAS  | 3. See HAS, revert to NOTHING
 */

/*  SPAWN PROJECTILE PROCEDURE
 *   
 *          Source                    |          Target
 *    1. Check for 1 tank             | 1. Only tank nearby
 *    2. REQ current HAS from Target  | 2. Ack req with SENDthing to target
 *    3. Go to send procedure         | 3. Handle cooldown if applicable
 */

/*  SEND PROJECTILE PROCEDURE
 *   
 *          Source          |          Target
 *    1. SENDthing on face  | 1. Ack SENDthing with RCVthing on face
 *    2. Ack RCV with NONE  | 2. See NONE on face, handle HAS
 */


void setup() {
  randomize();
  tankTimeoutTimer.set(1);
  waitingToTransfer = false;
  startGameTimer.set(STARTGAMEDELAY);
}

void loop() {

  if (startGameTimer.isExpired() && objectState == NOTHING){
    checkForZombieSpawn();
  }
  
  // Handle Object States
  switch (objectState) {
    case NOTHING:
      checkCycleObject();
      triggerTankRequest();
      switch (projectileState) {
        case NONE:
          checkProjectileSpawn();
        break;
        case HASBULLET:
          checkProjectileSend();
        break;
        case HASROCKET:
          checkProjectileSend();
        break;
      }
      checkZombiePull();
      break;
    case HASWALL:
      checkCycleObject();
      checkWallRegen();
      checkToggleSpawning();
      checkForMelee();
      break;
    case HASTANK:
      checkTankRequest(); // Look for REQTANK
      if (projectileState == ISUNDEAD) {
        checkForMelee();
        biteTheLiving();
      } else {
        checkCycleObject();
        checkCycleProjectile();
      }
      break;
    case REQTANK:
      checkForExit(); // Look for GIVETANK# at tankSource
      break;
    case GIVETANK3:
      checkForArrival(); // Look for HASTANK at tankDirection
      break;
    case GIVETANK2:
      checkForArrival(); // Look for HASTANK at tankDirection
      break;
    case GIVETANK1:
      checkForArrival(); // Look for HASTANK at tankDirection
      break;
  }

  // Handle Projectile movemement
  switch (projectileState) {
    case SENDBULLET:
      checkForProjectileReceive();
      break;
    case RCVBULLET:
      checkProjectileGone();
      break;
    case SENDROCKET:
      checkForProjectileReceive();
      break;
    case RCVROCKET:
      checkProjectileGone();
      break;
  }

  checkProjectileIncoming();

  commsHandler();

  damageHandler();
  
  displayHandler();
}

void checkCycleObject() {
  switch (objectState) {
    case NOTHING:
      if (buttonLongPressed()) {
//        objectState = HASWALL;
//        projectileState = NONE;
//        objectHealth = WALLHEALTH;
        objectState = HASTANK;
        projectileState = HASBULLET;
        objectHealth = 3;
      }
      break;
    case HASWALL:
      if (buttonLongPressed()) {
//        objectState = HASTANK;
//        projectileState = HASBULLET;
//        objectHealth = 3;
        objectState = NOTHING;
        projectileState = NONE;
        objectHealth = -1;
        startGameTimer.set(STARTGAMEDELAY);
      }
      break;
    case HASTANK:
      if (buttonLongPressed()) {
//        objectState = NOTHING;
//        projectileState = NONE;
//        objectHealth = -1;
        objectState = HASWALL;
        projectileState = NONE;
        objectHealth = WALLHEALTH;
        wallSpawning = false;
      }
      break;
  }
}

void checkWallRegen(){
  if ((objectHealth < WALLHEALTH) && wallSpawning) {
    if (wasWallDamaged && wallRegenTimer.isExpired()){
      objectHealth++;
      wasWallDamaged = false;
    } else if (wallRegenTimer.isExpired()){
      wasWallDamaged = true;
      wallRegenTimer.set(WALLREGENMS);
    }
  }
}

void checkZombiePull(){
  int zombieDirection = -1;
  int countHumans = 0;
  FOREACH_FACE(f){
    if (!isValueReceivedOnFaceExpired(f) && (getObjectStateFromDirection(f) == HASTANK)) {
      if (!(getProjectileStateFromDirection(f) == ISUNDEAD)){
        countHumans++;
      } else if ((getProjectileStateFromDirection(f) == ISUNDEAD)) {
        zombieDirection = f;
      }
    }
  }
  if (zombieDirection >= 0) {
    zombieWalk = zombieWalk - (random(MAXHUNGER - MINHUNGER) + MINHUNGER) - (countHumans * HUMANSCENT);
  } else {
    zombieWalk = SHAMBLEDELAY;
  }
  if (zombieWalk < 0) {
    //Trigger zombie request!
    tankSource = zombieDirection;
    objectState = REQTANK;
    zombieWalk = SHAMBLEDELAY;
  }
}

void checkForMelee() {
  if (buttonDoubleClicked()){
    bool playersAdjacent = false;
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f) && (getObjectStateFromDirection(f) == HASTANK) && !(getProjectileStateFromDirection(f) == ISUNDEAD)) {
        playersAdjacent = true;
      }
    }
    if (playersAdjacent) {
      objectHealth = objectHealth - MELEEDAMAGE;
      tempProjectileStorage = ISUNDEAD;
      damageHandler();
    }
  }
}

void biteTheLiving() {
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f) && (biteTarget < 0) && (getObjectStateFromDirection(f) == HASTANK) && !(getProjectileStateFromDirection(f) == ISUNDEAD)) {
      if (!attacking) {
        biteTimer.set(BITEDELAYMS);
        attacking = true;
        biteTarget = f;
      } else {
        if (biteTimer.isExpired()) {
        attacking = false;
        }
      }
    }
  }
}

void checkForZombieSpawn(){
  bool spawnerAdjacent = false;
  bool noTanksAdjacent = true;
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f) && getObjectStateFromDirection(f) == HASWALL) {
      spawnerAdjacent = true;
    } else if (!isValueReceivedOnFaceExpired(f) && getObjectStateFromDirection(f) == HASTANK && getProjectileStateFromDirection(f) == ISUNDEAD){
      noTanksAdjacent = false;
    }
  }
  if (spawnerAdjacent && noTanksAdjacent && (random(SPAWNMAX) < SPAWNTRIGGER)) {
    objectState = HASTANK;
    projectileState = ISUNDEAD;
    objectHealth = 3;
  }
}

void checkCycleProjectile () {
  if (buttonSingleClicked()) {
    // ammo = 6; // Maybe later
    
    // Old Projectile Code
//    if (projectileState == HASBULLET) {
//      projectileState = HASROCKET;
//    } else {
//      projectileState = HASBULLET;
//    }
  }
}

void checkToggleSpawning () {
  if (buttonSingleClicked()) {
    wallSpawning = !wallSpawning;
  }
}

void triggerTankRequest() {
  if (buttonSingleClicked() && isOnlyOneTankAdjacent()) {
    tankSource = wheresTheTank();
    objectState = REQTANK;
  }
}

void checkProjectileSpawn () {
  if (buttonDoubleClicked() && isOnlyOneTankAdjacent()) {
    projectileSource = wheresTheTank();
    projectileTarget = (projectileSource + 3) % 6;
    if (getProjectileStateFromDirection(projectileSource) == HASBULLET) {
      projectileState = HASBULLET;
      startProjectile();
//    } else if (getProjectileStateFromDirection(projectileSource) == HASROCKET) {
//      projectileState = HASROCKET;
//      startProjectile();
    }
  }  
}

void startProjectile () {
  if (objectState == NOTHING && projectileState == HASBULLET) {
    projectileTimer.set(BULLETDELAY);
//  } else if (objectState == NOTHING && projectileState == HASROCKET) {
//    projectileTimer.set(ROCKETDELAY);
  }
}

void checkProjectileSend () {
  if (objectState == NOTHING && projectileTimer.isExpired()) {
    if (projectileState == HASBULLET) {
      projectileState = SENDBULLET;
    }
//    if (projectileState == HASROCKET) {
//      projectileState = SENDROCKET;
//    }
  }
}

void checkProjectileIncoming () {
  if (objectState == NOTHING) {
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f) && getProjectileStateFromDirection(f) == SENDBULLET) {
        projectileState = RCVBULLET;
        projectileSource = f;
        projectileTarget = (projectileSource + 3) % 6;
//      } else if (!isValueReceivedOnFaceExpired(f) && getProjectileStateFromDirection(f) == SENDROCKET) {
//        projectileState = RCVROCKET;
//        projectileSource = f;
//        projectileTarget = (projectileSource + 3) % 6;
      }
    }
  } else if (objectState == HASTANK || objectState == HASWALL) {
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f) && getProjectileStateFromDirection(f) == SENDBULLET) {
        tempProjectileStorage = projectileState;
        projectileState = RCVBULLET;
        projectileSource = f;
      
//      } else if (!isValueReceivedOnFaceExpired(f) && getProjectileStateFromDirection(f) == SENDROCKET) {
//        tempProjectileStorage = projectileState;
//        projectileState = RCVROCKET;
//        projectileSource = f;
      }
    }
  }
// Simple collision handling code for later, possibly buggy

//  int projectiles = 0;
//
//  if (projectileState == ISUNDEAD){
//    projectiles = 4;
//  }
//  
//  FOREACH_FACE(f) {
//    if (!isValueReceivedOnFaceExpired(f) && getProjectileStateFromDirection(f) == SENDBULLET) {
//      projectiles = projectiles + 1;
//      projectileState = RCVBULLET;
//      projectileSource = f;
//      projectileTarget = (projectileSource + 3) % 6;
//    } else if (!isValueReceivedOnFaceExpired(f) && getProjectileStateFromDirection(f) == SENDROCKET) {
//      projectiles = projectiles + 10;
//      projectileState = RCVROCKET;
//      projectileSource = f;
//      projectileTarget = (projectileSource + 3) % 6;
//    }
//  }
//  if (projectiles > 10) {
//    handleBoom();
//  }
}

void handleBoom() {
  
}

void checkForProjectileReceive () {
  if (!isValueReceivedOnFaceExpired(projectileTarget)){
    if (objectState == NOTHING && (getProjectileStateFromDirection(projectileTarget) == RCVBULLET || getProjectileStateFromDirection(projectileTarget) == RCVROCKET)) {
      projectileState = NONE;
      projectileSource = -1;
      projectileTarget = -1;
    }
  } else {
    projectileState = NONE;
    projectileSource = -1;
    projectileTarget = -1;
  }
}

void checkProjectileGone () {
  if (!isValueReceivedOnFaceExpired(projectileSource) && getProjectileStateFromDirection(projectileSource) == ISUNDEAD){
    projectileState = NONE;
    projectileSource = -1;
    projectileTarget = -1;
  } else if (!isValueReceivedOnFaceExpired(projectileSource) && getProjectileStateFromDirection(projectileSource) == NONE) {
    if (projectileState == RCVBULLET && tempProjectileStorage == NONE) {
      projectileState = HASBULLET;
      startProjectile();
//    } else if (projectileState == RCVROCKET && tempProjectileStorage == NONE) {
//      projectileState = HASROCKET;
//      startProjectile();
    } else if (tempProjectileStorage != NONE) {
      projectileState = tempProjectileStorage;
      tempProjectileStorage = NONE;
    }
  }
}

void checkTankRequest() {
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f) && getObjectStateFromDirection(f) == REQTANK){
      tankDirection = f;
      if (objectHealth == 3) {
        objectState = GIVETANK3;
      } else if (objectHealth == 2) {
        objectState = GIVETANK2;
      } else {
        objectState = GIVETANK1;
      }
    break;
    }
  }
}

bool isOnlyOneTankAdjacent () {
  int numTanks = 0;
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f) && getObjectStateFromDirection(f) == HASTANK && getProjectileStateFromDirection(f) != ISUNDEAD){
      numTanks++;
    }
  }
  return (numTanks == 1);
}

int wheresTheTank () {
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f) && getObjectStateFromDirection(f) == HASTANK && getProjectileStateFromDirection(f) != ISUNDEAD){
      return f;
      break;
    }
  }
}

// Make sure the face you're checking actually has a Blink present _before_ calling this!
byte getObjectStateFromDirection (int dir) {
  return (getLastValueReceivedOnFace(dir) & 7);
}

// Make sure the face you're checking actually has a Blink present _before_ calling this!
byte getProjectileStateFromDirection (int dir) {
  return ((getLastValueReceivedOnFace(dir) >> 3) & 7);
}

void checkForExit() {
   if (!isValueReceivedOnFaceExpired(tankSource)){
     byte sourceState = getObjectStateFromDirection(tankSource);
     if (sourceState == GIVETANK1 || sourceState == GIVETANK2 || sourceState == GIVETANK3) {
      objectState = HASTANK;
      projectileState = getProjectileStateFromDirection(tankSource);
      tankSource = -1;
      
      waitingToTransfer = false;
      
      if (sourceState == GIVETANK3) {
        objectHealth = 3;
      } else if (sourceState == GIVETANK2) {
        objectHealth = 2;
      } else {
        objectHealth = 1;
      }
    }
  }
  transferTimeoutCheck();
}

void checkForArrival() {
  if (!isValueReceivedOnFaceExpired(tankDirection) && getObjectStateFromDirection(tankDirection) == HASTANK) {
    objectState = NOTHING;
    tankSource = -1;
    tankDirection = -1;
    objectHealth = -1;
    projectileState = NONE;
  }
  transferTimeoutCheck();
}

void endTransfer () {
    if (objectState == REQTANK){
      objectState = NOTHING;
      tankSource = -1;
      tankDirection = -1;
      objectHealth = -1;
      projectileState = NONE;
      tempProjectileStorage = NONE;
    }

    if (objectState == GIVETANK1 || objectState == GIVETANK2 || objectState == GIVETANK3) {
      objectState = HASTANK;
      tankSource = -1;
      tankDirection = -1;
    }
    
    waitingToTransfer = false;
}

void transferTimeoutCheck () {
  if ((objectState == REQTANK || objectState == GIVETANK1 || objectState == GIVETANK2 || objectState == GIVETANK3)){
    if (!waitingToTransfer) {
      tankTimeoutTimer.set(TANKMOVETIMEOUTMS);
      waitingToTransfer = true;
    } else if (tankTimeoutTimer.isExpired() && waitingToTransfer){
      waitingToTransfer = false;
      endTransfer ();
    }
  } else {
    waitingToTransfer = false;
  }
}

void damageHandler () {
    // Now that comms were sent, handle any damage to players/zombies
  if ( (objectState == HASTANK || objectState == HASWALL) && tempProjectileStorage != NONE) {
    if (projectileState == RCVBULLET){
      if (damageDelayTimer.isExpired()) {
        objectHealth--;
        damageDelayTimer.set(DAMAGEDELAYMS);
      }
//    } else if (projectileState == RCVROCKET){
//      if (damageDelayTimer.isExpired()) {
//        objectHealth = (objectHealth - 2);
//        damageDelayTimer.set(DAMAGEDELAYMS);
//      }
    }
    if (objectHealth <= 0) {
      objectState = NOTHING;
      projectileState = NONE;
      tempProjectileStorage = NONE;
      objectHealth = -1;
    }
    if (objectState != NOTHING) {
      projectileState = tempProjectileStorage;
      tempProjectileStorage = NONE;
    }
  }
}

void displayHandler () {
  switch (objectState) {
    case NOTHING:
      setColor(OFF);
        switch (projectileState) {
          case HASBULLET:
          if ((BULLETDELAY / 2) < projectileTimer.getRemaining()) {
            setColorOnFace(BULLETCOLOR, projectileSource);
          } else {
            setColorOnFace(BULLETCOLOR, projectileTarget);
          }
          break;
//          case HASROCKET:
//          if ((ROCKETDELAY / 2) < projectileTimer.getRemaining()) {
//            setColorOnFace(ROCKETCOLOR, projectileSource);
//          } else {
//            setColorOnFace(ROCKETCOLOR, projectileTarget);
//          }
//          break;
        }
      break;
    case HASWALL:
      FOREACH_FACE(f){
        if ( (f < objectHealth) && wallSpawning ) {
          setColorOnFace(WALLCOLOR,f);
        } else if ( (f < objectHealth) && !wallSpawning ){
          setColorOnFace(WALLOFFCOLOR,f);
        } else {
          setColorOnFace(OFF,f);
        }
      }
      break;
    case HASTANK:
      // Temp health state debug code
      if (objectHealth == 3){
        setColor(TANKCOLOR);
      } else if (objectHealth == 2){
        setColor(TANKTWOCOLOR);
      }else if (objectHealth == 1){
        setColor(TANKONECOLOR);
      } else {
        setColor(MAGENTA); // Something really wrong if we get here.
      }
      //Old Projectile Code
//      if (projectileState == HASBULLET) {
//        setColorOnFace(BULLETCOLOR, 0);
//      } else if (projectileState == HASROCKET) {
//        setColorOnFace(ROCKETCOLOR, 0);
//      }

      //Ammo code, eventually?
//      if ((projectileState != ISUNDEAD) && (ammo <= 0)) {
//        setColorOnFace (OFF, 0);
//      }
      if (projectileState == ISUNDEAD) {
        setColor(ZOMBIECOLOR);
        if (attacking) {
          setColorOnFace(makeColorRGB(255 - (biteTimer.getRemaining()/2 - 70), 0, 0),biteTarget);
        }
      }
      break;
    case REQTANK:
      setColor(OFF);
      //setColorOnFace(BLUE,tankSource);
      break;
    case GIVETANK3:
      setColor(OFF);
      //setColorOnFace(MAGENTA,tankDirection);
      break;
    case GIVETANK2:
      setColor(OFF);
      //setColorOnFace(MAGENTA,tankDirection);
      break;
    case GIVETANK1:
      setColor(OFF);
      //setColorOnFace(MAGENTA,tankDirection);
      break;
  }

  // Used to debug projectile transit
//  switch (projectileState) {
//    case SENDBULLET:
//      lightAllButTwo (MAGENTA, projectileSource, projectileTarget);
//      setColorOnFace(YELLOW,projectileTarget);
//      break;
//    case SENDROCKET:
//      lightAllButTwo (MAGENTA, projectileSource, projectileTarget);
//      setColorOnFace(YELLOW,projectileTarget);
//      break;
//    case RCVBULLET:
//      lightAllButTwo (CYAN, projectileSource, projectileTarget);
//      setColorOnFace(YELLOW,projectileSource);
//      break;
//    case RCVROCKET:
//      lightAllButTwo (CYAN, projectileSource, projectileTarget);
//      setColorOnFace(YELLOW,projectileSource);
//      break;
//  }
}

//
//void lightAllButTwo (Color lightColor, int firstFace, int secondFace) {
//  FOREACH_FACE(f) {
//    if (f != firstFace && f != secondFace) {
//      setColorOnFace(lightColor, f);
//    }
//  }
//}

void targetedSend(int targetFace, byte sendOne, byte sendOthers) {
  FOREACH_FACE(f) {
    if (f == targetFace) {
      setValueSentOnFace(sendOne, f);
    } else {
      setValueSentOnFace(sendOthers, f);
    }
  }
}

void commsHandler() {
    byte objectComms = objectState;
    byte targetComms = objectState;
    int targetFace = -1;
    bool sendingProjectile = false;
    
    switch (objectState) {
    case NOTHING:
      if (projectileState == SENDBULLET || projectileState == SENDROCKET) {
        sendingProjectile = true;
        targetFace = projectileTarget;
        targetComms = projectileState;
      }
//      if (projectileState == RCVBULLET || projectileState == RCVROCKET) {
//        sendingProjectile = true;
//        targetFace = projectileSource;
//        targetComms = projectileState;
//      }
      break;
    case HASWALL:
      if (wallSpawning){
        objectComms = HASWALL;
      } else {
        objectComms = NOTHING;
      }
      if (projectileState == RCVBULLET) { //|| projectileState == RCVROCKET) {
        sendingProjectile = true;
        targetFace = projectileSource;
        targetComms = projectileState;
      }
      break;
    case HASTANK:
      if (projectileState == RCVBULLET) { //|| projectileState == RCVROCKET) {
        sendingProjectile = true;
        targetFace = projectileSource;
        targetComms = projectileState;
      }
      if (projectileState == ISUNDEAD && attacking && biteTimer.isExpired()) {
        
        if ( getObjectStateFromDirection(biteTarget) == HASTANK && getProjectileStateFromDirection(biteTarget) != ISUNDEAD){
          sendingProjectile = true;
          targetFace = biteTarget;
          targetComms = SENDBULLET; // bite for one damage
        }
        biteTarget = -1;
      }
      break;
    case REQTANK:
      targetFace = tankSource;
      targetComms = objectState;
      objectComms = NOTHING;
      break;
    case GIVETANK3:
      targetFace = tankDirection;
      targetComms = objectState;
      objectComms = HASTANK;
      break;
    case GIVETANK2:
      targetFace = tankDirection;
      targetComms = objectState;
      objectComms = HASTANK;
      break;
    case GIVETANK1:
      targetFace = tankDirection;
      targetComms = objectState;
      objectComms = HASTANK;
      break;
    case HASBOOM:
      break;
  }

  if (targetFace < 0) {
    // Simple send, send same to all faces
    setValueSentOnAllFaces((projectileState << 3) + (objectComms));
  } else if (!sendingProjectile){
    // Targeted object state send, used for tank transit
    targetedSend(targetFace, ((projectileState << 3) + (targetComms)), ((projectileState << 3) + (objectComms)));
  } else {
    // Targeted projectile state send, used to send projectiles
    targetedSend(targetFace, ((targetComms << 3) + (objectComms)), ((NONE << 3) + (objectComms)));
  }  
}
