#include "Aimbot.hpp"


#include <chrono>
#include <cstddef>
#include <iostream>


#include "data.hpp"
#include "offsets.hpp"


Vector3 Aimbot::aimAnglesTo(Vector3& target)
{
    //Vector3 localPosition = localPlayer.getAbsolutePosition();
    Vector3 localPosition = localPlayer.getHeadPosition();
	
    Vector3 punchAngles = localPlayer.getAimPunch();

    Vector3 dPosition = localPosition - target;

    float hypotenuse = sqrt(dPosition(0) * dPosition(0) + dPosition(1) * dPosition(1));

    Vector3 a((float)(atan2f(dPosition(2), hypotenuse) * 57.295779513082f), (float)(atanf(dPosition(1) / dPosition(0)) * 57.295779513082f), 0);

    if (dPosition(0) >= 0.f)
        a(1) += 180.0f;

    Vector3 aimAngles;
    aimAngles(0) = a(0);     // up and down
    aimAngles(1) = a(1);      // left and right

    aimAngles(0) -= punchAngles(0) * 2;
    aimAngles(1) -= punchAngles(1) * 2;

    normalizeAngles(aimAngles);
    clampAngles(aimAngles);

    aimAngles(2) = 0.f;
    return aimAngles;
}

Vector3 Aimbot::angleDifferenceToEntity(Entity& localPlayer, Entity& entity, uint32_t boneId)
{
    Vector3 viewAngles = getViewAngles();
    Vector3 pos = entity.getHeadPosition();//entity.getBonePosition(boneId);

    Vector3 aimAngles = aimAnglesTo(pos);

    Vector3 dAngle(-1, -1, 0);

    if (aimAngles(0) != aimAngles(0) || aimAngles(1) != aimAngles(1))
        return dAngle;


    dAngle(0) = abs(aimAngles(0) - viewAngles(0));
    dAngle(1) = abs(aimAngles(1) - viewAngles(1));

    return dAngle;
}

Entity Aimbot::findClosestEnemyToFOV(float fov, uint32_t boneId)
{
    uint32_t localPlayerTeam = localPlayer.getTeam();
    Entity closestPlayer;

    if (!localPlayer.isValidPlayer())
        return closestPlayer;

    Vector3 localPosition = localPlayer.getAbsolutePosition();

    float closest = 999999999.f;
    for (int i = 0; i < 64; i++)
    {
        int EntityAddr = Driver.ReadVirtualMemory<int>(ProcessId, ClientAddress + hazedumper::signatures::dwEntityList + i * 0x10, sizeof(int));

        if (EntityAddr == NULL)
        {
            continue;
        }

        Entity entity = Entity(EntityAddr);

        if (!entity.isValidPlayer())
            continue;

        if (entity.getTeam() == localPlayerTeam)
            continue;

        Vector3 entityPosition = entity.getBonePosition(boneId);

        if (!bspParser->is_visible(localPosition, entityPosition))
            continue;

        Vector3 dAngle = angleDifferenceToEntity(localPlayer, entity, boneId);
        float screenDifferenceToEntity = sqrt(dAngle(0) * dAngle(0) + dAngle(1) * dAngle(1));
        if (screenDifferenceToEntity >= closest)
            continue;

        if (screenDifferenceToEntity >= fov) 
        {
            //std::cout << fov << " " << screenDifferenceToEntity << std::endl;
            continue;
        }

        closest = screenDifferenceToEntity;
        closestPlayer = entity;
    }

    return closestPlayer;
}

Vector3 Aimbot::getViewAngles()
{
    int clientState = Driver.ReadVirtualMemory<int>(ProcessId, EngineAddress + hazedumper::signatures::dwClientState, sizeof(int));
	return Driver.ReadVirtualMemory<Vector3>(ProcessId, clientState + hazedumper::signatures::dwClientState_ViewAngles, sizeof(Vector3));
}

void Aimbot::setViewAngles(Vector3& viewAngles)
{
    int clientState = Driver.ReadVirtualMemory<int>(ProcessId, EngineAddress + hazedumper::signatures::dwClientState, sizeof(int));
    Driver.WriteVirtualMemory(ProcessId, clientState + hazedumper::signatures::dwClientState_ViewAngles, viewAngles, sizeof(viewAngles));
}

void Aimbot::setSensitivity(float sens)
{
    uint32_t sensitivityPtr = Driver.ReadVirtualMemory<uint32_t>(ProcessId, ClientAddress + hazedumper::signatures::dwSensitivityPtr, sizeof(uint32_t));
    uint32_t sensitivity = *reinterpret_cast<uint32_t*>(&sens) ^ sensitivityPtr;

    Driver.WriteVirtualMemory(ProcessId, ClientAddress + hazedumper::signatures::dwSensitivity, sensitivity, sizeof(sensitivity));
}

float Aimbot::getSensitivity()
{
    uint32_t sensitivityPtr = Driver.ReadVirtualMemory<uint32_t>(ProcessId, ClientAddress + hazedumper::signatures::dwSensitivityPtr, sizeof(uint32_t));
    uint32_t sensitivity = Driver.ReadVirtualMemory<uint32_t>(ProcessId, ClientAddress + hazedumper::signatures::dwSensitivity, sizeof(uint32_t));

    sensitivity ^= sensitivityPtr;

    float sens = *reinterpret_cast<float*>(&sensitivity);

    return sens;
}

void Aimbot::resetSensitivity()
{
    setSensitivity(defaultSensitivity);
}

const char* Aimbot::getMapDirectory()
{
    int clientState = Driver.ReadVirtualMemory<int>(ProcessId, EngineAddress + hazedumper::signatures::dwClientState, sizeof(int));
    static std::array<char, 0x120> mapDirectory = Driver.ReadVirtualMemory<std::array<char, 0x120>>(ProcessId, clientState + hazedumper::signatures::dwClientState_MapDirectory, sizeof(std::array<char, 0x120>));
    return mapDirectory.data();
}

const char* Aimbot::getGameDirectory()
{
    int clientState = Driver.ReadVirtualMemory<int>(ProcessId, EngineAddress + hazedumper::signatures::dwClientState, sizeof(int));
    static std::array<char, 0x120> gameDirectory = Driver.ReadVirtualMemory<std::array<char, 0x120>>(ProcessId, clientState + hazedumper::signatures::dwGameDir, sizeof(std::array<char, 0x120>));
    return gameDirectory.data();
}


bool Aimbot::aimAssist(float fov, int boneId)
{
    const char* gameDirectory = getGameDirectory();
    const char* mapDirectory = getMapDirectory();
    bspParser->parse_map(gameDirectory, mapDirectory);

    if (!localPlayer.isValidPlayer())
        return false;


    if (localPlayer.getShotsFired() < 3)  // aimbot begins at 3rd bullet
        return false;

    static Entity target = findClosestEnemyToFOV(fov, boneId);
    static auto killTime = std::chrono::high_resolution_clock::now();
    static Vector3* lastPosition = NULL;

    Entity newTarget = findClosestEnemyToFOV(fov, boneId);
    if (target.isValidPlayer() && !target.isInAir() && newTarget.isValidPlayer() && target.getBase() == newTarget.getBase())
    {
        killTime = std::chrono::high_resolution_clock::now();
        lastPosition = new Vector3();

        // body aim if low
        if (target.getHealth() < 50)
            *lastPosition = target.getHeadPosition();//target.getBonePosition(CHEST_BONE_ID);
        else
            *lastPosition = target.getHeadPosition();//target.getBonePosition(boneId);
    }

    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> timeSinceKill = now - killTime;

    // continue shooting at target even after they die for 0.2 seconds
    if (timeSinceKill.count() > 0.2f && lastPosition)
    {
        delete lastPosition;
        lastPosition = NULL;
    }

    if (lastPosition)
    {
        if (!bspParser->is_visible(localPlayer.getAbsolutePosition(), *lastPosition))
            return false;

        // block user input
        setSensitivity(0.f);

        Vector3 targetAngle = aimAnglesTo(*lastPosition);
        if (targetAngle(0) != targetAngle(0) || targetAngle(1) != targetAngle(1) || targetAngle(2) != targetAngle(2))
            return false;

        Vector3 viewAngles = getViewAngles();


        float dYawTowardsRight = viewAngles(1) - targetAngle(1);
        while (dYawTowardsRight < 0)
            dYawTowardsRight += 360.f;
        while (dYawTowardsRight > 360.f)
            dYawTowardsRight -= 360.f;
        float dYawTowardsLeft = dYawTowardsRight - 360.f;

        Vector3 dAngles = targetAngle - viewAngles;
        if (abs(dYawTowardsRight) < abs(dYawTowardsLeft))
            dAngles(1) = -dYawTowardsRight;
        else
            dAngles(1) = -dYawTowardsLeft;

        Vector3 smoothTargetAngles = viewAngles + dAngles / 5;

        normalizeAngles(smoothTargetAngles);
        clampAngles(smoothTargetAngles);

        smoothTargetAngles(2) = 0.f;
        setViewAngles(smoothTargetAngles);

        return true;
    }
    else
    {
        // find new target
        target = newTarget;
        killTime = std::chrono::high_resolution_clock::now();
        lastPosition = NULL;
        resetSensitivity();

        return false;
    }
}

void Aimbot::aimBot(float fov, int boneId)
{
    if (!localPlayer.isValidPlayer())
        return;

    Entity target = findClosestEnemyToFOV(fov, boneId);

    if (!target.isValidPlayer())
        return;

    Vector3 pos = target.getHeadPosition();//target.getBonePosition(boneId); TODO changed

    Vector3 aimAngles = aimAnglesTo(pos);
    if (aimAngles(0) != aimAngles(0) || aimAngles(1) != aimAngles(1) || aimAngles(2) != aimAngles(2))
        return;

    //std::cout << aimAngles(0) << "-" << aimAngles(1) << "-" << aimAngles(2) << std::endl;
    //aimAngles(0) += 5;
    setViewAngles(aimAngles);
    //localPlayer.shoot();
}

bool Aimbot::enemyIsInCrossHair()
{
    if (!localPlayer.isValidPlayer())
        return false;
    uint32_t crossHairId = localPlayer.getCrosshairId();
    if (crossHairId <= 0 || crossHairId > 65)
        return false;

    crossHairId -= 1;

    int EntityAddr = Driver.ReadVirtualMemory<int>(ProcessId, ClientAddress + hazedumper::signatures::dwEntityList + 0x10 * crossHairId, sizeof(int));

    Entity target = Entity(EntityAddr);

    if (!target.isValidPlayer())
        return false;

    bool isEnemy = target.getTeam() != localPlayer.getTeam();

    return isEnemy;
}

void Aimbot::inCrossTriggerBot()
{
    if (enemyIsInCrossHair())
        localPlayer.shoot();
    else
        localPlayer.setForceAttack(4);
}

void Aimbot::walkBot()
{

    if (!localPlayer.isValidPlayer())
        return;

    Vector3 viewAngles = getViewAngles();
    Vector3 velocity = localPlayer.getVelocity();
    float speed = sqrt(velocity(0) * velocity(0) + velocity(1) * velocity(1));

    if (speed < 150.f)
    {
        viewAngles(0) = 0.f;
        viewAngles(1) += 1.f;
        viewAngles(2) = 0.f;

        normalizeAngles(viewAngles);
        clampAngles(viewAngles);

        setViewAngles(viewAngles);
    }
}

void Aimbot::normalizeAngles(Vector3& angles)
{
    while (angles(0) > 89.f)
        angles(0) -= 180.f;

    while (angles(0) < -89.f)
        angles(0) += 180.f;

    while (angles(1) > 180.f)
        angles(1) -= 360.f;

    while (angles(1) < -180.f)
        angles(1) += 360.f;
}

void Aimbot::clampAngles(Vector3& angles)
{
    if (angles(0) > 89.0)
        angles(0) = 89.0;

    if (angles(0) < -89.0)
        angles(0) = -89.0;

    if (angles(1) > 180.0)
        angles(1) = 180.0;

    if (angles(1) < -180.0)
        angles(1) = -180.0;
}

Aimbot::Aimbot(hazedumper::BSPParser* bspParser)
{
    this->bspParser = bspParser;
    this->defaultSensitivity = getSensitivity();
}


Aimbot::Aimbot()
{
    
}

Aimbot::~Aimbot()
{

}
