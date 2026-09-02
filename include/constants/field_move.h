#ifndef GUARD_CONSTANTS_FIELD_MOVE_H
#define GUARD_CONSTANTS_FIELD_MOVE_H


enum FieldMove
{
    FIELD_MOVE_CUT,
    FIELD_MOVE_FLASH,
    FIELD_MOVE_ROCK_SMASH,
    FIELD_MOVE_STRENGTH,
    FIELD_MOVE_SURF,
    FIELD_MOVE_FLY,
    FIELD_MOVE_DIVE,
    FIELD_MOVE_WATERFALL,
    FIELD_MOVE_TELEPORT,
    FIELD_MOVE_DIG,
    FIELD_MOVE_SECRET_POWER,
    FIELD_MOVE_MILK_DRINK,
    FIELD_MOVE_SOFT_BOILED,
    FIELD_MOVE_SWEET_SCENT,
    FIELD_MOVE_ROCK_CLIMB,
    FIELD_MOVE_DEFOG,
    //HYDRA weather field moves -- hotbar-assignable like HMs, gated on a party mon knowing the move
    // + owning the matching TM. Using one sets the overworld weather (which carries into battle).
    FIELD_MOVE_RAIN_DANCE,
    FIELD_MOVE_SUNNY_DAY,
    FIELD_MOVE_SANDSTORM,
    FIELD_MOVE_HAIL,
    FIELD_MOVES_COUNT
};


#endif //GUARD_CONSTANTS_FIELD_MOVE_H
