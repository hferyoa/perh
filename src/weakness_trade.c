#include "global.h"
#include "pokemon.h"
#include "string_util.h"
#include "battle_main.h"
#include "move.h"
#include "constants/moves.h"
#include "constants/species.h"
#include "constants/pokemon.h"
#include "constants/abilities.h"
#include "weakness_trade.h"

// ── Configuration ───────────────────────────────────────────────────────────
// If TRUE, condition B requires that the lead has NO resisted (not very
// effective) moves against the candidate either — not just no SE moves.
// If FALSE, condition B only blocks candidates where the lead has a SE move.
#define WEAKNESS_TRADE_REQUIRE_NO_RESISTED  FALSE

// ── Ability immunity table ───────────────────────────────────────────────────
// Maps ability → the move type it makes the holder immune to.
static const struct {
    u16 ability;
    u8  immuneType;
} sAbilityImmunities[] = {
    { ABILITY_LEVITATE,        TYPE_GROUND   },
    { ABILITY_FLASH_FIRE,      TYPE_FIRE     },
    { ABILITY_WATER_ABSORB,    TYPE_WATER    },
    { ABILITY_DRY_SKIN,        TYPE_WATER    },
    { ABILITY_STORM_DRAIN,     TYPE_WATER    },
    { ABILITY_VOLT_ABSORB,     TYPE_ELECTRIC },
    { ABILITY_MOTOR_DRIVE,     TYPE_ELECTRIC },
    { ABILITY_LIGHTNING_ROD,   TYPE_ELECTRIC },
    { ABILITY_SAP_SIPPER,      TYPE_GRASS    },
    { ABILITY_EARTH_EATER,     TYPE_GROUND   },
    { ABILITY_WELL_BAKED_BODY, TYPE_FIRE     },
    { ABILITY_WIND_RIDER,      TYPE_FLYING   },
};

// Returns TRUE if the given ability grants immunity to moveType.
static bool32 AbilityGrantsImmunity(u16 ability, u8 moveType)
{
    u32 i;
    for (i = 0; i < ARRAY_COUNT(sAbilityImmunities); i++)
    {
        if (sAbilityImmunities[i].ability == ability
         && sAbilityImmunities[i].immuneType == moveType)
            return TRUE;
    }
    return FALSE;
}

// Returns TRUE if any of the candidate species' three ability slots grant
// immunity to moveType.
static bool32 SpeciesAbilityBlocksType(u16 species, u8 moveType)
{
    u32 i;
    for (i = 0; i < 3; i++)
    {
        if (AbilityGrantsImmunity(gSpeciesInfo[species].abilities[i], moveType))
            return TRUE;
    }
    return FALSE;
}

// Returns TRUE if the lead's ability is a Mold Breaker variant that ignores
// ability-based immunities.
static bool32 LeadHasMoldBreaker(u16 leadAbility)
{
    return (leadAbility == ABILITY_MOLD_BREAKER
         || leadAbility == ABILITY_TURBOBLAZE
         || leadAbility == ABILITY_TERAVOLT);
}

// Returns TRUE if the candidate species has Dazzling or Queenly Majesty,
// which block priority moves from affecting them.
static bool32 SpeciesHasPriorityBlock(u16 species)
{
    u32 i;
    for (i = 0; i < 3; i++)
    {
        u16 ability = gSpeciesInfo[species].abilities[i];
        if (ability == ABILITY_DAZZLING || ability == ABILITY_QUEENLY_MAJESTY)
            return TRUE;
    }
    return FALSE;
}

// ── Type effectiveness ───────────────────────────────────────────────────────
// gTypeEffectivenessTable is declared in include/battle_main.h.
// Indexed [attackType][defType], stores UQ_4_12 values:
//   0x0000 = immune, 0x0800 = 0.5×, 0x1000 = 1×, 0x2000 = 2×
#define TYPE_EFFECTIVENESS_NEUTRAL  UQ_4_12(1.0)  // 0x1000

static u16 MulTypeModifier(u16 a, u16 b)
{
    return (u16)UQ_4_12_TO_INT((u32)a * b + UQ_4_12_ROUND);
}

// Returns the combined UQ_4_12 type modifier for moveType vs defType1/defType2.
static u16 GetCombinedTypeModifier(u8 moveType, u8 defType1, u8 defType2)
{
    u16 mod = gTypeEffectivenessTable[moveType][defType1];
    if (defType2 != defType1)
        mod = MulTypeModifier(mod, gTypeEffectivenessTable[moveType][defType2]);
    return mod;
}

// ── Move validity ────────────────────────────────────────────────────────────
// Returns TRUE if a move should be considered for type matchup evaluation:
// must be a damaging move with non-fixed damage.
static bool32 IsDamagingMove(u16 move)
{
    if (gMovesInfo[move].power == 0)
        return FALSE;
    if (gMovesInfo[move].effect == EFFECT_FIXED_DAMAGE_ARG
     || gMovesInfo[move].effect == EFFECT_LEVEL_DAMAGE
     || gMovesInfo[move].effect == EFFECT_OHKO)
        return FALSE;
    return TRUE;
}

// ── Learnset collection ──────────────────────────────────────────────────────
// Mirrors GiveBoxMonInitialMoveset: collects the last MAX_MON_MOVES unique
// moves learned at or below `level`, skipping level-0 entries.
static void CollectLevelUpMoves(u16 species, u8 level, u16 outMoves[MAX_MON_MOVES])
{
    const struct LevelUpMove *learnset = GetSpeciesLevelUpLearnset(species);
    u16 moves[MAX_MON_MOVES] = {MOVE_NONE, MOVE_NONE, MOVE_NONE, MOVE_NONE};
    u8 addedMoves = 0;
    s32 i, j;

    for (i = 0; learnset[i].move != LEVEL_UP_MOVE_END; i++)
    {
        bool32 alreadyKnown = FALSE;

        if (learnset[i].level > level)
            break;
        if (learnset[i].level == 0)
            continue;

        for (j = 0; j < addedMoves; j++)
        {
            if (moves[j] == learnset[i].move)
            {
                alreadyKnown = TRUE;
                break;
            }
        }

        if (!alreadyKnown)
        {
            if (addedMoves < MAX_MON_MOVES)
                moves[addedMoves++] = learnset[i].move;
            else
            {
                for (j = 0; j < MAX_MON_MOVES - 1; j++)
                    moves[j] = moves[j + 1];
                moves[MAX_MON_MOVES - 1] = learnset[i].move;
            }
        }
    }

    for (i = 0; i < MAX_MON_MOVES; i++)
        outMoves[i] = moves[i];
}

// ── Core search ──────────────────────────────────────────────────────────────
static u16 FindWeaknessTradeReplacement(u16 startSpecies, u8 level,
                                  u8 leadType1, u8 leadType2,
                                  const u16 leadMoves[MAX_MON_MOVES],
                                  u16 leadAbility)
{
    u16 species;
    u16 candMoves[MAX_MON_MOVES];
    u8 m;
    u32 i;
    bool32 leadIgnoresAbilities = LeadHasMoldBreaker(leadAbility);

    // Pre-compute which type the lead is immune to via its ability.
    u8 leadAbilityImmuneType = TYPE_NONE;
    for (i = 0; i < ARRAY_COUNT(sAbilityImmunities); i++)
    {
        if (sAbilityImmunities[i].ability == leadAbility)
        {
            leadAbilityImmuneType = sAbilityImmunities[i].immuneType;
            break;
        }
    }

    for (species = startSpecies + 1; species < NUM_SPECIES; species++)
    {
        if (gSpeciesInfo[species].natDexNum == NATIONAL_DEX_NONE)
            continue;

        u8 candType1 = gSpeciesInfo[species].types[0];
        u8 candType2 = gSpeciesInfo[species].types[1];
        bool32 candHasPriorityBlock = SpeciesHasPriorityBlock(species);

        // ── Condition A: candidate has a SE move vs the lead ──
        CollectLevelUpMoves(species, level, candMoves);

        bool32 candCanHit = FALSE;
        for (m = 0; m < MAX_MON_MOVES; m++)
        {
            if (candMoves[m] == MOVE_NONE)
                continue;
            if (!IsDamagingMove(candMoves[m]))
                continue;
            u8 moveType = gMovesInfo[candMoves[m]].type;
            // Lead's ability may grant immunity to this move type.
            if (moveType == leadAbilityImmuneType)
                continue;
            u16 mod = GetCombinedTypeModifier(moveType, leadType1, leadType2);
            if (mod > TYPE_EFFECTIVENESS_NEUTRAL)
            {
                candCanHit = TRUE;
                break;
            }
        }
        if (!candCanHit)
            continue;

        // ── Condition B: lead has no qualifying move vs the candidate ──
        bool32 leadCanHit = FALSE;
        for (m = 0; m < MAX_MON_MOVES; m++)
        {
            if (leadMoves[m] == MOVE_NONE)
                continue;
            if (!IsDamagingMove(leadMoves[m]))
                continue;

            // Dazzling/Queenly Majesty blocks priority moves.
            if (candHasPriorityBlock && gMovesInfo[leadMoves[m]].priority > 0)
                continue;

            u8 moveType = gMovesInfo[leadMoves[m]].type;

            // Check ability immunity on the candidate (unless lead has
            // Mold Breaker / Turboblaze / Teravolt).
            if (!leadIgnoresAbilities && SpeciesAbilityBlocksType(species, moveType))
                continue;

            u16 mod = GetCombinedTypeModifier(moveType, candType1, candType2);

#if WEAKNESS_TRADE_REQUIRE_NO_RESISTED
            if (mod != TYPE_EFFECTIVENESS_NEUTRAL)
            {
                leadCanHit = TRUE;
                break;
            }
#else
            if (mod > TYPE_EFFECTIVENESS_NEUTRAL)
            {
                leadCanHit = TRUE;
                break;
            }
#endif
        }
        if (leadCanHit)
            continue;

        return species;
    }
    return SPECIES_NONE;
}

// ── Script-callable functions ─────────────────────────────────────────────────

// WeaknessTradeSearch:
// Searches for a replacement for the lead Pokémon.
// On success:
//   gStringVar1        = lead species name
//   gStringVar2        = replacement species name
//   gSpecialVar_0x8000 = replacement species ID
//   gSpecialVar_Result = TRUE
// On failure:
//   gSpecialVar_Result = FALSE
void WeaknessTradeSearch(void)
{
    struct Pokemon *lead = &gPlayerParty[0];

    u16 leadSpecies = GetMonData(lead, MON_DATA_SPECIES, NULL);
    u8  level       = GetMonData(lead, MON_DATA_LEVEL, NULL);
    u8  abilityNum  = GetMonData(lead, MON_DATA_ABILITY_NUM, NULL);
    u16 leadAbility = gSpeciesInfo[leadSpecies].abilities[abilityNum];
    u8  leadType1   = gSpeciesInfo[leadSpecies].types[0];
    u8  leadType2   = gSpeciesInfo[leadSpecies].types[1];

    u16 leadMoves[MAX_MON_MOVES];
    u8 m;
    for (m = 0; m < MAX_MON_MOVES; m++)
        leadMoves[m] = GetMonData(lead, MON_DATA_MOVE1 + m, NULL);

    u16 newSpecies = FindWeaknessTradeReplacement(leadSpecies, level,
                                           leadType1, leadType2,
                                           leadMoves, leadAbility);

    if (newSpecies == SPECIES_NONE)
    {
        gSpecialVar_Result = FALSE;
        return;
    }

    // Store found species for WeaknessTradeExecute.
    gSpecialVar_0x8000 = newSpecies;
    gSpecialVar_Result = TRUE;

    // Copy species names into string vars for NPC dialogue.
    StringCopy(gStringVar1, GetSpeciesName(leadSpecies));
    StringCopy(gStringVar2, GetSpeciesName(newSpecies));
}

// WeaknessTradeExecute:
// Performs the actual trade using the species stored by WeaknessTradeSearch.
// Preserves the lead's OT ID, OT name, and OT gender on the new Pokémon.
void WeaknessTradeExecute(void)
{
    struct Pokemon *lead = &gPlayerParty[0];
    u16 newSpecies = gSpecialVar_0x8000;
    u8  level      = GetMonData(lead, MON_DATA_LEVEL, NULL);

    // Capture OT data before overwriting slot 0.
    u32 otId = GetMonData(lead, MON_DATA_OT_ID, NULL);
    u8  otName[PLAYER_NAME_LENGTH + 1];
    GetMonData(lead, MON_DATA_OT_NAME, otName);
    u8  otGender = GetMonData(lead, MON_DATA_OT_GENDER, NULL);

    CreateMon(lead, newSpecies, level, USE_RANDOM_IVS,
              FALSE, 0, OT_ID_PRESET, otId);

    SetMonData(lead, MON_DATA_OT_NAME, otName);
    SetMonData(lead, MON_DATA_OT_GENDER, &otGender);
}