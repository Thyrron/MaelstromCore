/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * Ported from ArkCORE-NG (https://github.com/Arkania/ArkCORE-NG, src/server/scripts/Maelstrom/zone_lost_isles.cpp),
 * GPL-2.0, by the Arkania team (gpn39f, ladel and others). Copyright (C) 2011-2017 ArkCORE <http://www.arkania.net/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* The Lost Isles (map 648, zone 4720), the Goblin starting zone from Don't Go Into the Light! to Old Friends.
 * Stages 8-10 (Old Friends to Victory!) have no ArkCORE scripts, nor any in other open source; theirs, at the end of
 * this file, are MaelstromCore's own, written from the 4.3.4 client's spells and TDB 434's data.
 *
 * Rewritten against AzerothCore's APIs. Differences from ArkCORE that are on purpose:
 * - The stages come from phase_area (build_world_db.py goblin(), LOST_ISLES_STAGES); ArkCORE's QUEST_PHASE auras and
 *   SetInPhase calls are gone. The core refreshes a player's phases on every quest state change.
 * - Talk() uses TDB 434's creature_text groups, picked by their text (ArkCORE's group ids belonged to its own
 *   database); lines TDB has no creature_text for are sent by their broadcast text id, and lines with neither are
 *   left out.
 * - ArkCORE's waypoint paths were in its own database, which is not available. Rides and walks go straight to where
 *   the quest continues (the next quest NPC's spawn or ArkCORE's own end point), with mmaps path finding on the ground.
 * - The raptor trap's effects (Trading Up) are done by the raptor itself; ArkCORE drove them through the trap's state.
 * - The five Oomlot-defence NPCs share one AI (they stand still, defend the town in stage 181 and walk home).
 */

#include "CellImpl.h"
#include "CombatAI.h"
#include "Containers.h"
#include "CreatureScript.h"
#include "GameObjectAI.h"
#include "GameObjectScript.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ItemScript.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerScript.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "TemporarySummon.h"
#include "Vehicle.h"
#include <array>
#include <set>
#include <vector>

namespace LostIsles
{

// A quest giver's gossip option that hands back a lost quest item or vehicle (defined with its table below)
bool HandleLostIslesGossipSpell(Player* player, uint32 menu, uint32 option);

enum LostIslesQuests
{
    QUEST_GOBLIN_ESCAPE_PODS_F      = 14001,
    QUEST_GET_OUR_STUFF_BACK        = 14014,
    QUEST_MONKEY_BUSINESS           = 14019,
    QUEST_MINER_TROUBLES            = 14021,
    QUEST_CAPTURING_THE_UNKNOWN     = 14031,
    QUEST_THE_ENEMY_OF_MY_ENEMY     = 14234,
    QUEST_THE_VICIOUS_VALE          = 14235,
    QUEST_WEED_WHACKER              = 14236,
    QUEST_FORWARD_MOVEMENT          = 14237,
    QUEST_INFRARED_INFRADEAD        = 14238,
    QUEST_DONT_GO_INTO_THE_LIGHT    = 14239,
    QUEST_TO_THE_CLIFFS             = 14240,
    QUEST_GET_TO_THE_GYROCHOPPA     = 14241,
    QUEST_PRECIOUS_CARGO            = 14242,
    QUEST_WARCHIEFS_REVENGE         = 14243,
    QUEST_UP_UP_AND_AWAY            = 14244,
    QUEST_ITS_A_TOWN_IN_A_BOX       = 14245,
    QUEST_HELP_WANTED               = 14248,
    QUEST_BACK_TO_AGGRA             = 14303,
    QUEST_MEET_ME_UP_TOP            = 14326,
    QUEST_ITS_OUR_PROBLEM_NOW       = 14473,
    QUEST_GOBLIN_ESCAPE_PODS_M      = 14474,
    QUEST_CLUSTER_CLUCK             = 24671,
    QUEST_TRADING_UP                = 24741,
    QUEST_THE_BIGGEST_EGG_EVER      = 24744,
    QUEST_A_GOBLIN_IN_SHARKS_CLOTHING = 24817,
    QUEST_INVASION_IMMINENT         = 24856,
    QUEST_NAGA_HIDE                 = 24859,
    QUEST_IRRESISTIBLE_POOL_PONY    = 24864,
    QUEST_SURRENDER_OR_ELSE         = 24868,
    QUEST_GET_BACK_TO_TOWN          = 24897,
    QUEST_TOWN_IN_A_BOX_UNDER_ATTACK = 24901,
    QUEST_OOMLOT_VILLAGE            = 24924,
    QUEST_FREE_THE_CAPTIVES         = 24925,
    QUEST_OOMLOT_DEALT_WITH         = 24937,
    QUEST_UP_THE_VOLCANO            = 24940,
    QUEST_ZOMBIES_VS_ROCKET_BOOTS   = 24942,
    QUEST_THREE_LITTLE_PYGMIES      = 24945,
    QUEST_ROCKIN_POWDER             = 24946,
    QUEST_ROCKET_BOOT_BOOST         = 24952,
    QUEST_VOLCANOTH                 = 24958,
    QUEST_OLD_FRIENDS               = 25023,
    QUEST_MINE_DISPOSAL             = 25058,
    QUEST_LETS_RIDE                 = 25100,
    QUEST_MORALE_BOOST              = 25122,
    QUEST_LIGHT_AT_THE_END_OF_THE_TUNNEL = 25125,
    QUEST_SHREDDER_SHUTDOWN         = 25200,
    QUEST_WHAT_KIND_OF_NAME_IS_CHIP = 25203,
    QUEST_THE_ULTIMATE_FOOTBOMB_UNIFORM = 25201,
    QUEST_RELEASE_THE_VALVES        = 25204,
    QUEST_THE_SLAVE_PITS            = 25213,
    QUEST_ESCAPE_VELOCITY           = 25214,
    QUEST_FINAL_CONFRONTATION       = 25251,
    QUEST_HOBART_NEEDS_YOU          = 27139,
};

enum LostIslesCreatures
{
    NPC_GOBLIN_SURVIVOR             = 34748,
    NPC_FOREMAN_DAMPWICK            = 35769,
    NPC_MONKEY_BUSINESS_CREDIT      = 35760,
    NPC_MINER_TROUBLES_CREDIT       = 35816,
    NPC_KILAG_GOREFANG              = 35893,
    NPC_ORC_SCOUT                   = 35894,
    NPC_POISON_SPITTER              = 35896,
    NPC_FREEZYA                     = 35897,
    NPC_ORC_SCOUT_LEAVING           = 35918,
    NPC_STRANGLE_VINE               = 35995,
    NPC_ORC_SCOUT_FREED             = 36042,
    NPC_ORC_SCOUT_INFRARED          = 36100,
    NPC_SCOUT_BRAX                  = 36112,
    NPC_THRALL_CAGED                = 36145,
    NPC_THRALL_UP_TOP               = 36161,
    NPC_ALLIANCE_SAILOR             = 36176,
    NPC_THRALL_CAMP                 = 36188,
    NPC_FOREMAN_DAMPWICK_CAMP       = 36470,
    NPC_GEARGRINDER_GIZMO           = 36600,
    NPC_DOC_ZAPNOZZLE               = 36608,
    NPC_CAPTURING_BUNNY_1           = 37872,
    NPC_CAPTURING_BUNNY_2           = 37895,
    NPC_CAPTURING_BUNNY_3           = 37896,
    NPC_CAPTURING_BUNNY_4           = 37897,
    NPC_ELM_GENERAL_PURPOSE_BUNNY   = 23837,
    NPC_FIREWORKS_BUNNY             = 24021,
    NPC_CLUSTER_CLUCK_CREDIT        = 38117,
    NPC_HOBART_GRAPPLEHAMMER        = 38120,
    NPC_BAMM_MEGABOMB               = 38122,
    NPC_ASSISTANT_GREELY            = 38124,
    NPC_SPINY_RAPTOR                = 38187,
    NPC_WILD_CLUCKER_EGG            = 38195,
    NPC_VASHJELAN_SIREN             = 38360,
    NPC_GOBLIN_SURVIVOR_TOWN        = 38409,
    NPC_NAGA_HATCHLING_CREDIT       = 38413,
    NPC_FACELESS_OF_THE_DEEP        = 38448,
    NPC_ACE                         = 38455,
    NPC_NAGA_HATCHLING_SURRENDER    = 38457,
    NPC_BC_ELIMINATOR               = 38526,
    NPC_OOMLOT_WARRIOR              = 38531,
    NPC_KEZAN_CITIZEN_TOWN          = 38745,
    NPC_GOBLIN_ZOMBIE               = 38753,
    NPC_ZOMBIE_KILL_CREDIT          = 38807,
    NPC_GAAHL                       = 38808,
    NPC_MALMO                       = 38809,
    NPC_TELOCH                      = 38810,
    NPC_GOBLIN_CAPTIVE_GAAHL        = 38812,
    NPC_GOBLIN_ZOMBIE_NORD          = 38813,
    NPC_GOBLIN_ZOMBIE_SOR           = 38815,
    NPC_GOBLIN_ZOMBIE_OST           = 38816,
    NPC_GOBLIN_CAPTIVE_TELOCH       = 50310,
    NPC_GOBLIN_CAPTIVE_MALMO        = 50311,
    NPC_NAGA_HATCHLING_CLICKABLE    = 44579,
};

enum LostIslesObjects
{
    GO_ARCANE_CAGE                  = 195704,
    GO_RAPTOR_TRAP                  = 201972,
    GO_THE_BIGGEST_EGG_EVER         = 201977,
};

enum LostIslesSpells
{
    SPELL_COSMETIC_STATE_DEATH      = 81238,
    SPELL_JUMPER_CABLES             = 69022,
    // Don't Go Into The Light! (14239), the client's own spells for the scene: the master spell triggers the placement
    // teleport onto the beach, whose script effect lays the player low with Near Death! (stun, invisible to others,
    // its light); the player then summons their own Doc (visible to them only), who talks with Gizmo through the force
    // cast and the two dummies, offers the quest, and wakes them with Quest Complete, the quest's reward spell. The
    // player's Despawn Doc Zapnozzle sends him off again.
    SPELL_DGITL_MASTER              = 69283,
    SPELL_DGITL_PLACEMENT_TELEPORT  = 69040,
    SPELL_NEAR_DEATH                = 69010,
    SPELL_DGITL_SUMMON_DOC          = 69018,
    SPELL_DGITL_FORCE_CAST_TO_PLAYER = 69085,
    SPELL_DGITL_DUMMY_TO_GIZMO      = 69086,
    SPELL_DGITL_DUMMY_BACK_TO_DOC   = 68257,
    SPELL_DGITL_QUEST_COMPLETE      = 69013,
    SPELL_DGITL_DESPAWN_DOC         = 69043,
    SPELL_THERMOHYDRAULIC_FLIPPERS_VISUAL = 68256,
    SPELL_THERMOHYDRAULIC_FLIPPERS  = 68258,
    SPELL_TRADE_PRINCE_CONTROLLER_AURA = 67433,
    SPELL_SUMMON_TRADE_PRINCE_GALLYWIX = 67845,
    SPELL_SUMMON_LIVE_GOBLIN_SURVIVOR = 66137,
    SPELL_SUMMONS_CONTROLLER        = 66136,
    SPELL_SWIM                      = 37744,
    SPELL_FEIGN_DEATH               = 29266,
    SPELL_ZOMBIE_TARGETS_ROCKET_BOOTS = 72886,
    SPELL_ZOMBIE_TARGETS_SHOOT      = 72944,
};

// Broadcast texts for lines that TDB 434 has no creature_text for
enum LostIslesBroadcastTexts
{
    BCT_DOC_GIZMO_WHAT_ARE_YOU_DOING = 36543,
    BCT_DOC_THATS_PLAYER            = 36544,
    BCT_DOC_STAY_BACK               = 36545,
    BCT_DOC_CLEAR                   = 36546,
    BCT_DOC_DONT_GO_INTO_THE_LIGHT  = 36547,
    BCT_DOC_RIGHT_CHOICE            = 36548,
    BCT_DOC_MORE_SURVIVORS          = 36549,
    BCT_GIZMO_SORRY_DOC             = 36577,
    BCT_ACE_SHORT_LEASH             = 38356,
    BCT_ACE_READY                   = 38357,
    BCT_ACE_OUT_OF_HERE             = 38358,
};

// The players around an object (ArkCORE's FindNearestPlayers)
std::list<Player*> GetPlayersInRange(WorldObject* source, float range)
{
    std::list<Player*> players;
    Acore::AnyPlayerInObjectRangeCheck checker(source, range);
    Acore::PlayerListSearcher<Acore::AnyPlayerInObjectRangeCheck> searcher(source, players, checker);
    Cell::VisitObjects(source, searcher, range);
    return players;
}

// A random creature of one of these entries around the source (ArkCORE's FillTargetList)
Creature* SelectRandomCreatureInRange(WorldObject* source, std::vector<uint32> const& entries, float range)
{
    std::list<Creature*> creatures;
    source->GetCreatureListWithEntryInGrid(creatures, { Acore::Containers::SelectRandomContainerElement(entries) },
        range);
    if (creatures.empty())
        return nullptr;
    return Acore::Containers::SelectRandomContainerElement(creatures);
}

bool IsZombie(uint32 entry)
{
    return entry == NPC_GOBLIN_ZOMBIE || entry == NPC_GOBLIN_ZOMBIE_NORD || entry == NPC_GOBLIN_ZOMBIE_SOR
        || entry == NPC_GOBLIN_ZOMBIE_OST;
}

/*######
## Stage 1 (phase 170): the beach, the escape pods, the kaja'mite mine and the Vicious Vale
######*/

/*######
## 36129 Gyrochoppa Pilot: "Hey! Get away from my flying machine!" when attacked (ArkCORE-NG, GPL-2.0)
######*/

struct npc_lost_isles_gyrochoppa_pilot : public ScriptedAI
{
    npc_lost_isles_gyrochoppa_pilot(Creature* creature) : ScriptedAI(creature) { }

    void JustEngagedWith(Unit* who) override
    {
        if (Player* player = who ? who->ToPlayer() : nullptr)
            Talk(0, player);
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }
};

/*######
## 36608 Doc Zapnozzle: every new arrival's own Doc (Summon Doc Zapnozzle 69018, seen by them only) swims up from the
## sea, wakes them with his jumper cables and lets them choose to live (quest 14239)
######*/

enum DocZapnozzle
{
    EVENT_DOC_WATCH_PATIENT         = 1,
    EVENT_DOC_WELCOME_1,
    EVENT_DOC_WELCOME_2,
    EVENT_DOC_WELCOME_3,
    EVENT_DOC_WELCOME_4,
    EVENT_DOC_WELCOME_5,
    EVENT_DOC_WELCOME_6,
    EVENT_DOC_WELCOME_7,
    EVENT_DOC_WELCOME_8,
    EVENT_DOC_REVIVED_1,
    EVENT_DOC_REVIVED_2,

    POINT_DOC_BARREL                = 1,
    ZONE_THE_LOST_ISLES             = 4720,
};

// The barrel by the raft that he stands on: ArkCORE-NG's spawn of him (GPL-2.0), which matches retail screenshots
Position const DocBarrelPos = { 537.445f, 3272.11f, 0.185513f, 3.20316f };
// Where he comes up, 20 yards out in the water: MaNGOS Three's one spawn of him, a sniffed summon ((c) the MaNGOS
// authors, CC BY-NC-SA 3.0). Summon Doc Zapnozzle aims at a nearby creature that no source names.
Position const DocSummonPos = { 551.461f, 3260.25f, -0.566483f, 2.4277f };

// The player's own Doc
Creature* FindDocOf(Player* player)
{
    std::list<Creature*> docs;
    player->GetCreatureListWithEntryInGrid(docs, NPC_DOC_ZAPNOZZLE, 60.0f);
    for (Creature* doc : docs)
        if (doc->IsAlive() && doc->IsSummon() && doc->ToTempSummon()->GetSummonerGUID() == player->GetGUID())
            return doc;
    return nullptr;
}

// Not woken yet: Don't Go Into the Light! is the Lost Isles' first quest; characters that went on to the escape pods
// before the scene existed are left alone
bool NeedsWaking(Player* player)
{
    return !player->IsGameMaster() && player->GetZoneId() == ZONE_THE_LOST_ISLES
        && !player->IsQuestRewarded(QUEST_DONT_GO_INTO_THE_LIGHT)
        && player->GetQuestStatus(QUEST_GOBLIN_ESCAPE_PODS_F) == QUEST_STATUS_NONE
        && player->GetQuestStatus(QUEST_GOBLIN_ESCAPE_PODS_M) == QUEST_STATUS_NONE;
}

// The player summons their Doc a few seconds after landing, once the movie of Kezan's end is over
void ScheduleDocSummon(Player* player, Milliseconds delay)
{
    player->m_Events.AddEventAtOffset([player]()
    {
        if (!player->HasAura(SPELL_NEAR_DEATH) || !NeedsWaking(player) || FindDocOf(player))
            return;
        if (player->GetMovie())
            ScheduleDocSummon(player, 1s);
        else
            player->CastSpell(player, SPELL_DGITL_SUMMON_DOC, true);
    }, delay);
}

// On landing from Kezan and on every login until woken: the master spell (the placement teleport and Near Death!,
// spell_lost_isles_dgitl_placement), or only the Doc when they still lie near death
void StartDocScene(Player* player)
{
    if (!NeedsWaking(player) || FindDocOf(player))
        return;

    player->m_Events.AddEventAtOffset([player]()
    {
        if (!NeedsWaking(player) || FindDocOf(player))
            return;
        if (player->HasAura(SPELL_NEAR_DEATH))
            ScheduleDocSummon(player, 2s);
        else
            player->CastSpell(player, SPELL_DGITL_MASTER, true);
    }, 1s);
}

struct npc_lost_isles_doc_zapnozzle : public ScriptedAI
{
    npc_lost_isles_doc_zapnozzle(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        Player* player = summoner ? summoner->ToPlayer() : nullptr;
        if (!player)
        {
            me->DespawnOrUnsummon();
            return;
        }

        _patientGUID = player->GetGUID();
        // The quest is offered once he has done his work
        me->RemoveNpcFlag(NPCFlags(UNIT_NPC_FLAG_QUESTGIVER | UNIT_NPC_FLAG_GOSSIP));
        me->SetReactState(REACT_PASSIVE);
        // Only his patient sees him, but the sharks would still go for him in the water
        me->SetImmuneToNPC(true);
        // Near Death! hides the patient with invisibility type 7 (100000); the Doc sees through it to treat them
        me->m_invisibilityDetect.AddFlag(INVISIBILITY_UNK7);
        me->m_invisibilityDetect.SetValue(INVISIBILITY_UNK7, 1000000);
        me->SetWalk(false);
        // His movement flags are ours (CREATURE_FLAG_EXTRA_NO_MOVE_FLAGS_UPDATE): the barrel has no collision and
        // stands in the water, where the core would keep him swimming. He swims over and climbs onto it.
        me->SetSwim(true);
        me->GetMotionMaster()->MovePoint(POINT_DOC_BARREL, DocBarrelPos, false);
        _events.ScheduleEvent(EVENT_DOC_WATCH_PATIENT, 2s);
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE || id != POINT_DOC_BARREL)
            return;

        // Standing on the barrel: no swimming, held at its height
        me->SetSwim(false);
        me->SetDisableGravity(true);
        if (Player* patient = ObjectAccessor::GetPlayer(*me, _patientGUID))
            me->SetFacingToObject(patient);
        _events.ScheduleEvent(EVENT_DOC_WELCOME_1, 1s);
    }

    void sQuestReward(Player* player, Quest const* quest, uint32 /*opt*/) override
    {
        if (quest->GetQuestId() != QUEST_DONT_GO_INTO_THE_LIGHT || player->GetGUID() != _patientGUID)
            return;

        // The quest's reward spell, Quest Complete, has woken them (spell_lost_isles_dgitl_quest_complete)
        me->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
        me->Say(BCT_DOC_RIGHT_CHOICE, player);
        _events.ScheduleEvent(EVENT_DOC_REVIVED_1, 5s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            Player* patient = ObjectAccessor::GetPlayer(*me, _patientGUID);
            if (!patient || !patient->IsInWorld() || !patient->IsWithinDistInMap(me, 60.0f))
            {
                // Logged out or gone: the next login summons a new Doc
                me->DespawnOrUnsummon();
                return;
            }

            switch (eventId)
            {
                case EVENT_DOC_WATCH_PATIENT:
                    _events.ScheduleEvent(EVENT_DOC_WATCH_PATIENT, 2s);
                    break;
                case EVENT_DOC_WELCOME_1:
                    me->Say(BCT_DOC_GIZMO_WHAT_ARE_YOU_DOING, patient);
                    _events.ScheduleEvent(EVENT_DOC_WELCOME_2, 5s);
                    break;
                case EVENT_DOC_WELCOME_2:
                    me->Say(BCT_DOC_THATS_PLAYER, patient);
                    _events.ScheduleEvent(EVENT_DOC_WELCOME_3, 5s);
                    break;
                case EVENT_DOC_WELCOME_3:
                    // Doc to the player, the player to Gizmo, who answers and turns back to the Doc
                    // (spell_lost_isles_dgitl_dummy_to_gizmo)
                    DoCast(patient, SPELL_DGITL_FORCE_CAST_TO_PLAYER, true);
                    _events.ScheduleEvent(EVENT_DOC_WELCOME_4, 5s);
                    break;
                case EVENT_DOC_WELCOME_4:
                    me->Say(BCT_DOC_STAY_BACK, patient);
                    _events.ScheduleEvent(EVENT_DOC_WELCOME_5, 2s);
                    break;
                case EVENT_DOC_WELCOME_5:
                    DoCast(patient, SPELL_JUMPER_CABLES, true);
                    _events.ScheduleEvent(EVENT_DOC_WELCOME_6, 5s);
                    break;
                case EVENT_DOC_WELCOME_6:
                    me->Say(BCT_DOC_CLEAR, patient);
                    DoCast(patient, SPELL_JUMPER_CABLES, true);
                    _events.ScheduleEvent(EVENT_DOC_WELCOME_7, 5s);
                    break;
                case EVENT_DOC_WELCOME_7:
                    DoCast(patient, SPELL_JUMPER_CABLES, true);
                    _events.ScheduleEvent(EVENT_DOC_WELCOME_8, 5s);
                    break;
                case EVENT_DOC_WELCOME_8:
                    me->Say(BCT_DOC_DONT_GO_INTO_THE_LIGHT, patient);
                    // The quest completes itself (QuestType 0): an exclamation mark, then straight to the reward
                    me->SetNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
                    patient->PlayerTalkClass->SendQuestGiverStatus(patient->GetQuestDialogStatus(me), me->GetGUID());
                    break;
                case EVENT_DOC_REVIVED_1:
                    me->Say(BCT_DOC_MORE_SURVIVORS, patient);
                    _events.ScheduleEvent(EVENT_DOC_REVIVED_2, 5s);
                    break;
                case EVENT_DOC_REVIVED_2:
                    patient->CastSpell(patient, SPELL_DGITL_DESPAWN_DOC, true);
                    // In case the spell found no Doc to send off
                    me->DespawnOrUnsummon(2s);
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
    ObjectGuid _patientGUID;
};

class player_lost_isles_dont_go_into_the_light : public PlayerScript
{
public:
    player_lost_isles_dont_go_into_the_light() : PlayerScript("player_lost_isles_dont_go_into_the_light",
        { PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_UPDATE_ZONE }) { }

    void OnPlayerLogin(Player* player) override
    {
        // Before the scene was rebuilt the quest was taken into the log; it never is now
        if (player->GetZoneId() == ZONE_THE_LOST_ISLES && !player->IsQuestRewarded(QUEST_DONT_GO_INTO_THE_LIGHT)
            && player->GetQuestStatus(QUEST_DONT_GO_INTO_THE_LIGHT) != QUEST_STATUS_NONE)
            player->RemoveActiveQuest(QUEST_DONT_GO_INTO_THE_LIGHT);
        StartDocScene(player);
    }

    void OnPlayerUpdateZone(Player* player, uint32 newZone, uint32 /*newArea*/) override
    {
        if (newZone == ZONE_THE_LOST_ISLES)
            StartDocScene(player);
    }
};

/*######
## 36600 Geargrinder Gizmo: hands out the flippers for the escape pods (quests 14001, 14474)
######*/

enum GeargrinderGizmo
{
    SAY_GIZMO_ESCAPE_PODS           = 0,
};

struct npc_lost_isles_geargrinder_gizmo : public ScriptedAI
{
    npc_lost_isles_geargrinder_gizmo(Creature* creature) : ScriptedAI(creature) { }

    void sQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() != QUEST_GOBLIN_ESCAPE_PODS_F && quest->GetQuestId() != QUEST_GOBLIN_ESCAPE_PODS_M)
            return;

        player->CastSpell(player, SPELL_THERMOHYDRAULIC_FLIPPERS_VISUAL, true);
        player->CastSpell(player, SPELL_THERMOHYDRAULIC_FLIPPERS, true);
        player->CastSpell(player, SPELL_TRADE_PRINCE_CONTROLLER_AURA, true);
        Talk(SAY_GIZMO_ESCAPE_PODS, player);
    }
};

/*######
## 195188 Goblin Escape Pod: frees a survivor, the first one the Trade Prince himself
######*/

struct go_lost_isles_escape_pod : public GameObjectAI
{
    go_lost_isles_escape_pod(GameObject* go) : GameObjectAI(go) { }

    bool GossipHello(Player* player, bool reportUse) override
    {
        // The 4.3.4 client reports the use when its Opening cast (3365, 1 s) begins; the pod opens when the cast is done
        if (reportUse)
            return false;

        if (player->GetQuestStatus(QUEST_GOBLIN_ESCAPE_PODS_M) != QUEST_STATUS_INCOMPLETE
            && player->GetQuestStatus(QUEST_GOBLIN_ESCAPE_PODS_F) != QUEST_STATUS_INCOMPLETE)
            return false;

        // The survivor comes out of this pod: the summons' nearby-entry destination is the escape pod (conditions)

        if (player->HasAura(SPELL_TRADE_PRINCE_CONTROLLER_AURA))
        {
            player->RemoveAurasDueToSpell(SPELL_TRADE_PRINCE_CONTROLLER_AURA);
            player->CastSpell(player, SPELL_SUMMON_TRADE_PRINCE_GALLYWIX, true);
        }
        else
            player->CastSpell(player, SPELL_SUMMON_LIVE_GOBLIN_SURVIVOR, true);

        player->CastSpell(player, SPELL_SUMMONS_CONTROLLER, true);
        player->KilledMonsterCredit(NPC_GOBLIN_SURVIVOR);
        me->DespawnOrUnsummon();
        return true;
    }
};

// The flippers and the Trade Prince's controller are for the swim to the pods only: Sassy takes them back when the
// quest is handed in (npc_lost_isles_sassy_hardwrench_beach), and they go when it is abandoned
void RemoveEscapePodFlippers(Player* player)
{
    player->RemoveAurasDueToSpell(SPELL_THERMOHYDRAULIC_FLIPPERS);
    player->RemoveAurasDueToSpell(SPELL_THERMOHYDRAULIC_FLIPPERS_VISUAL);
    player->RemoveAurasDueToSpell(SPELL_TRADE_PRINCE_CONTROLLER_AURA);
}

class player_lost_isles_escape_pods : public PlayerScript
{
public:
    player_lost_isles_escape_pods() : PlayerScript("player_lost_isles_escape_pods", { PLAYERHOOK_ON_QUEST_ABANDON }) { }

    void OnPlayerQuestAbandon(Player* player, uint32 questId) override
    {
        if (questId == QUEST_GOBLIN_ESCAPE_PODS_F || questId == QUEST_GOBLIN_ESCAPE_PODS_M)
            RemoveEscapePodFlippers(player);
    }
};

/*######
## 34748 Goblin Survivor, 35649 Trade Prince Gallywix: out of the pod, a word and off to the shore
######*/

enum PodSurvivor
{
    EVENT_POD_SURVIVOR_TALK         = 1,
    EVENT_POD_SURVIVOR_SWIM,
    EVENT_POD_SURVIVOR_DESPAWN,

    SAY_POD_SURVIVOR_FIRST          = 0,
    SAY_POD_SURVIVOR_LAST           = 6,
    NPC_POD_GALLYWIX                = 35649,

    POINT_POD_SURVIVOR_SHORE        = 1,
    POINT_POD_SURVIVOR_CAMP         = 2,
};

// How far under the water's surface a freed survivor swims
constexpr float POD_SURVIVOR_DEPTH = 1.0f;

struct npc_lost_isles_pod_survivor : public ScriptedAI
{
    npc_lost_isles_pod_survivor(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        // The pods float on the surface and the summon puts the survivor at the pod's height, so it swam in the air:
        // drop it into the water, about a yard under the surface like the sniffed Gallywix in the bay (z -0.92)
        float const waterZ = me->GetMap()->GetWaterLevel(me->GetPositionX(), me->GetPositionY());
        if (waterZ > INVALID_HEIGHT && me->GetPositionZ() > waterZ - POD_SURVIVOR_DEPTH)
            me->NearTeleportTo(me->GetPositionX(), me->GetPositionY(), waterZ - POD_SURVIVOR_DEPTH,
                me->GetOrientation());

        // A swimmer, not a flyer: without gravity and the swimming flag the client showed the swim pose in the air
        me->SetSwim(true);

        if (Player* player = summoner ? summoner->ToPlayer() : nullptr)
        {
            DoCastSelf(SPELL_SWIM, true);
            _playerGUID = player->GetGUID();
            _events.ScheduleEvent(EVENT_POD_SURVIVOR_TALK, 2s);
        }
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE)
            return;

        if (id == POINT_POD_SURVIVOR_SHORE)
            me->GetMotionMaster()->MovePoint(POINT_POD_SURVIVOR_CAMP, 594.541f, 3135.034f, -0.905f,
                FORCED_MOVEMENT_NONE, 0.0f, 0.0f, false);
        else if (id == POINT_POD_SURVIVOR_CAMP)
            me->DespawnOrUnsummon(2s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_POD_SURVIVOR_TALK:
                    if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
                    {
                        // The survivors have seven thanks and complaints; Gallywix has one line
                        if (me->GetEntry() == NPC_POD_GALLYWIX)
                            Talk(SAY_POD_SURVIVOR_FIRST, player);
                        else
                            Talk(urand(SAY_POD_SURVIVOR_FIRST, SAY_POD_SURVIVOR_LAST), player);
                    }
                    _events.ScheduleEvent(EVENT_POD_SURVIVOR_SWIM, 2s);
                    break;
                case EVENT_POD_SURVIVOR_SWIM:
                    // The retail sniff's route (as ShatterCore has it): swim to the shore, then run up to the camp
                    me->SetWalk(false);
                    me->GetMotionMaster()->MovePoint(POINT_POD_SURVIVOR_SHORE, 571.281f, 3181.45f, -2.889f,
                        FORCED_MOVEMENT_NONE, 0.0f, 0.0f, false);
                    _events.ScheduleEvent(EVENT_POD_SURVIVOR_DESPAWN, 30s); // failsafe
                    break;
                case EVENT_POD_SURVIVOR_DESPAWN:
                    me->DespawnOrUnsummon();
                    break;
                default:
                    break;
            }
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
};

/*######
## 35650 Sassy Hardwrench (the beach): quest talk; takes the flippers back when Goblin Escape Pods is handed in
######*/

enum SassyBeach
{
    SAY_SASSY_GET_OUR_STUFF_BACK    = 0,
    SAY_SASSY_HELP_WANTED           = 1,
    SAY_SASSY_ENEMY_OF_MY_ENEMY     = 2,
};

struct npc_lost_isles_sassy_hardwrench_beach : public ScriptedAI
{
    npc_lost_isles_sassy_hardwrench_beach(Creature* creature) : ScriptedAI(creature) { }

    void sQuestAccept(Player* player, Quest const* quest) override
    {
        switch (quest->GetQuestId())
        {
            case QUEST_GET_OUR_STUFF_BACK:
                Talk(SAY_SASSY_GET_OUR_STUFF_BACK, player);
                break;
            case QUEST_THE_ENEMY_OF_MY_ENEMY:
                Talk(SAY_SASSY_ENEMY_OF_MY_ENEMY, player);
                break;
            case QUEST_HELP_WANTED:
                Talk(SAY_SASSY_HELP_WANTED, player);
                break;
            default:
                break;
        }
    }

    void sQuestReward(Player* player, Quest const* quest, uint32 /*opt*/) override
    {
        if (quest->GetQuestId() == QUEST_GOBLIN_ESCAPE_PODS_F || quest->GetQuestId() == QUEST_GOBLIN_ESCAPE_PODS_M)
            RemoveEscapePodFlippers(player);
    }
};

/*######
## Quest givers whose quests only start or end with a line: 35758 Bamm Megabomb, 35786 Maxx Avalanche,
## 35875 Aggra, 36112 Scout Brax, 36470 Foreman Dampwick, 38381 Brett "Coins" McQuid; for stages 8 and 9 (own, TDB 434's
## lines): 38441 Ace, 39066 Kilag Gorefang, 39199 Assistant Greely
######*/

struct QuestLine
{
    uint32 entry;
    uint32 questId;
    bool onReward;
    uint8 group;
};

QuestLine const QuestLines[] =
{
    { 35758, QUEST_MONKEY_BUSINESS,         false, 0 },
    { 35758, QUEST_MONKEY_BUSINESS,         true,  1 },
    { 35786, QUEST_ITS_OUR_PROBLEM_NOW,     false, 0 },
    { 35786, QUEST_ITS_OUR_PROBLEM_NOW,     true,  1 },
    { 35875, QUEST_THE_VICIOUS_VALE,        false, 0 },
    { 35875, QUEST_FORWARD_MOVEMENT,        false, 1 },
    { 36112, QUEST_GET_TO_THE_GYROCHOPPA,   false, 0 },
    { 36470, QUEST_ITS_A_TOWN_IN_A_BOX,     false, 0 },
    { 38381, QUEST_NAGA_HIDE,               false, 0 },
    { 38441, QUEST_WHAT_KIND_OF_NAME_IS_CHIP, true, 0 },
    { 39066, QUEST_LETS_RIDE,               false, 0 },
    { 39199, QUEST_MORALE_BOOST,            false, 1 },
    { 39199, QUEST_LIGHT_AT_THE_END_OF_THE_TUNNEL, false, 3 },
};

void SayQuestLine(Creature* creature, Player* player, uint32 questId, bool onReward)
{
    for (QuestLine const& line : QuestLines)
        if (line.entry == creature->GetEntry() && line.questId == questId && line.onReward == onReward)
            creature->AI()->Talk(line.group, player);
}

struct npc_lost_isles_quest_talker : public ScriptedAI
{
    npc_lost_isles_quest_talker(Creature* creature) : ScriptedAI(creature) { }

    void sQuestAccept(Player* player, Quest const* quest) override
    {
        SayQuestLine(me, player, quest->GetQuestId(), false);
    }

    void sQuestReward(Player* player, Quest const* quest, uint32 /*opt*/) override
    {
        SayQuestLine(me, player, quest->GetQuestId(), true);
    }
};

/*######
## 36403 Trade Prince Gallywix (the beach): his speech to the survivors
######*/

enum GallywixBeach
{
    EVENT_GALLYWIX_SPEECH           = 1,
    SAY_GALLYWIX_SPEECH             = 0,
};

struct npc_lost_isles_gallywix_beach : public ScriptedAI
{
    npc_lost_isles_gallywix_beach(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _events.ScheduleEvent(EVENT_GALLYWIX_SPEECH, 45s, 60s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (_events.ExecuteEvent() == EVENT_GALLYWIX_SPEECH)
        {
            if (Player* player = me->SelectNearestPlayer(25.0f))
                Talk(SAY_GALLYWIX_SPEECH, player);
            _events.ScheduleEvent(EVENT_GALLYWIX_SPEECH, 45s, 60s);
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
};

/*######
## 34699 Bomb-Throwing Monkey: Monkey Business (14019) feeds it Nitro-Potassium Bananas
######*/

enum BombThrowingMonkey
{
    EVENT_MONKEY_THROW_BOMB         = 1,
    EVENT_MONKEY_ROCKET,
    EVENT_MONKEY_FALL,

    SPELL_COSMETIC_THROW_BOMB       = 66142,
    SPELL_NITRO_POTASSIUM_BANANAS   = 67917,
    SPELL_EXPLODING_BANANAS         = 67919,
};

struct npc_lost_isles_bomb_throwing_monkey : public ScriptedAI
{
    npc_lost_isles_bomb_throwing_monkey(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _eating = false;
        _events.ScheduleEvent(EVENT_MONKEY_THROW_BOMB, 30s, 45s);
    }

    void SpellHit(Unit* caster, SpellInfo const* spell) override
    {
        if (spell->Id != SPELL_NITRO_POTASSIUM_BANANAS || _eating)
            return;

        _eating = true;
        me->HandleEmoteCommand(EMOTE_ONESHOT_EAT);
        _events.Reset();
        _events.ScheduleEvent(EVENT_MONKEY_ROCKET, 1s);
        if (Player* player = caster ? caster->ToPlayer() : nullptr)
            player->KilledMonsterCredit(NPC_MONKEY_BUSINESS_CREDIT);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_MONKEY_THROW_BOMB:
                    if (me->SelectNearestPlayer(frand(10.0f, 25.0f)))
                        DoCastSelf(SPELL_COSMETIC_THROW_BOMB);
                    _events.ScheduleEvent(EVENT_MONKEY_THROW_BOMB, 30s, 45s);
                    break;
                case EVENT_MONKEY_ROCKET:
                    // Exploding Bananas knocks it straight up (75 upward): about 4 s up and 4 s down
                    DoCastSelf(SPELL_EXPLODING_BANANAS, true);
                    _events.ScheduleEvent(EVENT_MONKEY_FALL, 8s);
                    break;
                case EVENT_MONKEY_FALL:
                    // back on the ground, dead (retail); it respawns as its spawn does
                    me->KillSelf();
                    break;
                default:
                    break;
            }
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
    bool _eating = false;
};

/*######
## 35769 Foreman Dampwick (the mine): Miner Troubles (14021) gives the player a frightened miner to escort, through the
## quest's own spells: accepting casts Quest Accept (68061), which puts the Frightened Miner Controller (68062) on the
## player, which summons the miner as their companion (68059); "I need another miner" casts New Miner from Gossip
## (68069); the reward spell, Quest Complete (68063), casts Despawn Frightened Miner (68060). The controller's end
## (abandoned, completed, replaced) takes the miner, his ore cart and its chunks away. Capturing the Unknown (14031)
## lets the player see the cave paintings.
######*/

enum ForemanDampwickMine
{
    SAY_DAMPWICK_MINER_TROUBLES     = 0,
    SAY_DAMPWICK_MINE_CLAIMED       = 1,
    NPC_FRIGHTENED_MINER_ESCORT     = 35813,
    NPC_KAJAMITE_CHUNK              = 35818,
    SPELL_DESPAWN_FRIGHTENED_MINER  = 68060,
    SPELL_MINER_QUEST_ACCEPT        = 68061,
    SPELL_FRIGHTENED_MINER_CONTROLLER = 68062,
    SPELL_NEW_MINER_FROM_GOSSIP     = 68069,
    SPELL_SEE_INVIS_AREA_MASTER     = 68920,
    SPELL_SEE_INVIS_BUNNY_1         = 70661,
    SPELL_SEE_INVIS_BUNNY_2         = 70678,
    SPELL_SEE_INVIS_BUNNY_3         = 70680,
    SPELL_SEE_INVIS_BUNNY_4         = 70681,
};

/*######
## 35813 Frightened Miner: Miner Troubles (14021), an escort. He walks the cavern on his own, from Kaja'mite Ore
## (195622) to Kaja'mite Ore, mines each with a line from the client, and the ore counts when he reaches the far
## chamber; the player keeps the monkeys off him. Route, stops, pauses and lines from the retail sniff as ShatterCore
## has them (Mr-Scooty, 2026-07-15; broadcast texts 36102, 36014, 36013, 36010, 36018), less a stray point that
## sent him down to the second ore and back before the first. He is the player's companion
## (68059). His ore cart (Summon Ore Cart, 68064; Ore Cart Transform, 68065; vehicle 115) trails him on its chain (Ore
## Cart Chain, 68122, which the cart channels on him), and each deposit he mines puts a Chunk of Kaja'mite (68072)
## into the cart (68074). After a fight he tells his controller and goes on (68071).
######*/

enum FrightenedMiner
{
    EVENT_MINER_SUMMON_CART         = 1,
    EVENT_MINER_CHAIN_CART,
    EVENT_MINER_START,
    EVENT_MINER_NEXT_POINT,
    EVENT_MINER_RUN_OFF,
    EVENT_MINER_DESPAWN,

    NPC_ORE_CART                    = 35814,
    SPELL_SUMMON_ORE_CART           = 68064,
    SPELL_ORE_CART_TRANSFORM        = 68065,
    SPELL_WHISPER_ON_COMBAT_RETURN  = 68071,
    SPELL_SUMMON_KAJAMITE_CHUNK     = 68072,
    SPELL_CHUNK_RIDE_ORE_CART       = 68074,
    SPELL_ORE_CART_CHAIN            = 68122,

    SAY_MINER_START                 = 0,
    SAY_MINER_CAVE_PAINTINGS        = 1,
    SAY_MINER_ACCOUNTANT            = 3,
    SAY_MINER_DONE                  = 4,
    SAY_MINER_JACKPOT               = 5,
    SAY_MINER_MOVE_ON               = 6,

    POINT_MINER_RUN_OFF             = 100,
};

struct MinerRoutePoint
{
    float X, Y, Z;
    uint32 PauseMs; // mining there, when not 0
    int8 Say;       // -1: none
};

std::array<MinerRoutePoint, 25> const MinerRoute =
{ {
    { 506.80f, 2976.93f,  7.14f,     0, -1 },
    { 516.90f, 2973.13f,  8.61f,     0, -1 },
    { 528.69f, 2961.12f,  6.86f,     0, -1 },
    { 538.82f, 2951.87f,  5.02f,     0, -1 },
    { 548.12f, 2943.13f,  2.30f,  3000, SAY_MINER_CAVE_PAINTINGS },   // the cavern's mouth (a look, no mining)
    { 557.69f, 2935.65f,  1.09f,     0, -1 },
    { 565.62f, 2936.04f,  0.30f,     0, -1 },
    { 572.24f, 2944.43f, -0.29f,     0, -1 },
    { 579.50f, 2956.21f, -1.52f,     0, -1 },
    { 587.78f, 2964.38f, -2.26f, 18000, SAY_MINER_ACCOUNTANT },       // the first ore
    { 589.45f, 2939.09f, -7.32f,     0, -1 },
    { 581.43f, 2923.87f, -6.39f,     0, -1 },
    { 575.18f, 2905.77f, -7.21f, 15000, SAY_MINER_JACKPOT },          // the second
    { 574.46f, 2890.24f, -7.65f,     0, -1 },
    { 592.10f, 2876.20f, -6.64f,     0, -1 },
    { 603.36f, 2865.64f, -6.54f,     0, -1 },
    { 607.68f, 2857.41f, -7.00f, 12000, SAY_MINER_MOVE_ON },          // the third
    { 620.28f, 2886.39f, -4.44f,     0, -1 },
    { 628.28f, 2911.83f, -1.83f,     0, -1 },
    { 635.80f, 2919.50f, -0.95f,     0, -1 },
    { 647.74f, 2927.78f, -0.02f,     0, -1 },
    { 658.50f, 2936.43f,  0.29f,     0, -1 },
    { 666.17f, 2947.39f,  0.12f, 10000, -1 },                         // the fourth, in the far chamber
    { 654.54f, 2974.84f,  1.53f,     0, -1 },
    { 660.71f, 2962.35f,  0.96f,     0, -1 },                         // the end: the ore is mined
} };

std::array<Position, 5> const MinerRunOff =
{ {
    { 654.05f, 2934.90f,  0.10f },
    { 632.76f, 2920.28f, -1.30f },
    { 622.06f, 2898.40f, -3.63f },
    { 613.32f, 2877.02f, -6.60f },
    { 603.03f, 2870.68f, -6.60f },
} };

struct npc_lost_isles_frightened_miner : public ScriptedAI
{
    npc_lost_isles_frightened_miner(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        _playerGUID = summoner ? summoner->GetGUID() : ObjectGuid::Empty;
        _point = 0;
        me->SetWalk(true);
        _events.ScheduleEvent(EVENT_MINER_SUMMON_CART, 1s);
    }

    void JustSummoned(Creature* summon) override
    {
        if (summon->GetEntry() != NPC_ORE_CART)
            return;

        _cartGUID = summon->GetGUID();
        summon->SetImmuneToNPC(true);
        summon->SetReactState(REACT_PASSIVE);
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE)
            return;

        if (id >= POINT_MINER_RUN_OFF)
        {
            uint32 const next = id - POINT_MINER_RUN_OFF + 1;
            if (next < MinerRunOff.size())
                me->GetMotionMaster()->MovePoint(POINT_MINER_RUN_OFF + next, MinerRunOff[next]);
            else
                me->DespawnOrUnsummon(200ms);
            return;
        }

        if (id != _point || id >= MinerRoute.size())
            return;

        _walking = false;

        MinerRoutePoint const& point = MinerRoute[id];
        Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID);
        if (point.Say >= 0 && player)
            Talk(uint8(point.Say), player);

        if (id + 1 == MinerRoute.size())
        {
            if (player)
            {
                Talk(SAY_MINER_DONE, player);
                player->KilledMonsterCredit(NPC_MINER_TROUBLES_CREDIT);
            }
            _events.ScheduleEvent(EVENT_MINER_RUN_OFF, 6s);
            return;
        }

        if (point.PauseMs && point.Say != SAY_MINER_CAVE_PAINTINGS)
            me->SetEmoteState(EMOTE_STATE_WORK_MINING);
        _events.ScheduleEvent(EVENT_MINER_NEXT_POINT, Milliseconds(500 + point.PauseMs));
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        me->SetEmoteState(EMOTE_ONESHOT_NONE);
    }

    // The fight over, he does not walk home: he tells his controller and takes up his leg, or his mining
    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        me->CombatStop(true);
        DoCastSelf(SPELL_WHISPER_ON_COMBAT_RETURN, true);
        if (_walking)
            MoveTo(_point);
        else
        {
            me->GetMotionMaster()->Clear();
            me->GetMotionMaster()->MoveIdle();
            MinerRoutePoint const& point = MinerRoute[_point];
            if (point.PauseMs && point.Say != SAY_MINER_CAVE_PAINTINGS)
                me->SetEmoteState(EMOTE_STATE_WORK_MINING);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_MINER_SUMMON_CART:
                    // An escort: the monkeys can hurt him (the core makes a companion immune to all after its summon),
                    // and he fights back what attacks him, with the player's help
                    me->SetImmuneToAll(false);
                    me->SetReactState(REACT_DEFENSIVE);
                    DoCastSelf(SPELL_SUMMON_ORE_CART, true);
                    _events.ScheduleEvent(EVENT_MINER_CHAIN_CART, 100ms);
                    break;
                case EVENT_MINER_CHAIN_CART:
                    if (Creature* cart = ObjectAccessor::GetCreature(*me, _cartGUID))
                    {
                        cart->CastSpell(cart, SPELL_ORE_CART_TRANSFORM, true);
                        cart->CastSpell(me, SPELL_ORE_CART_CHAIN, true);
                    }
                    // retail sets off some ten seconds after the miner appears
                    _events.ScheduleEvent(EVENT_MINER_START, 8s);
                    break;
                case EVENT_MINER_START:
                    if (Creature* cart = ObjectAccessor::GetCreature(*me, _cartGUID))
                        cart->GetMotionMaster()->MoveFollow(me, 2.0f, float(M_PI));
                    if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
                        Talk(SAY_MINER_START, player);
                    MoveTo(0);
                    break;
                case EVENT_MINER_NEXT_POINT:
                {
                    // not while he fights
                    if (me->IsInCombat())
                    {
                        _events.ScheduleEvent(EVENT_MINER_NEXT_POINT, 1s);
                        break;
                    }
                    me->SetEmoteState(EMOTE_ONESHOT_NONE);
                    // a deposit mined: its chunk goes into the cart
                    MinerRoutePoint const& mined = MinerRoute[_point];
                    if (mined.PauseMs && mined.Say != SAY_MINER_CAVE_PAINTINGS)
                        DoCastSelf(SPELL_SUMMON_KAJAMITE_CHUNK, true);
                    MoveTo(_point + 1);
                    break;
                }
                case EVENT_MINER_RUN_OFF:
                    me->SetEmoteState(EMOTE_ONESHOT_NONE);
                    me->CombatStop(true);
                    me->SetWalk(false);
                    me->GetMotionMaster()->MovePoint(POINT_MINER_RUN_OFF, MinerRunOff[0]);
                    _events.ScheduleEvent(EVENT_MINER_DESPAWN, 25s); // failsafe
                    break;
                case EVENT_MINER_DESPAWN:
                    me->DespawnOrUnsummon();
                    break;
                default:
                    break;
            }
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (Creature* cart = ObjectAccessor::GetCreature(*me, _cartGUID))
            cart->DespawnOrUnsummon(1200ms);
    }

private:
    void MoveTo(uint32 index)
    {
        _point = index;
        _walking = true;
        MinerRoutePoint const& point = MinerRoute[index];
        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MovePoint(index, point.X, point.Y, point.Z);
    }

    EventMap _events;
    ObjectGuid _playerGUID;
    ObjectGuid _cartGUID;
    uint32 _point = 0;
    bool _walking = false;
};

// The player's escorted miner, his ore cart and its chunks go (the controller's end: abandoned, completed, replaced)
void DespawnFrightenedMiner(Player* player)
{
    std::list<Creature*> miners;
    player->GetCreatureListWithEntryInGrid(miners, NPC_FRIGHTENED_MINER_ESCORT, 500.0f);
    for (Creature* miner : miners)
    {
        TempSummon* summon = miner->ToTempSummon();
        if (!summon || summon->GetSummonerGUID() != player->GetGUID())
            continue;

        for (uint32 entry : { uint32(NPC_KAJAMITE_CHUNK), uint32(NPC_ORE_CART) })
        {
            std::list<Creature*> things;
            miner->GetCreatureListWithEntryInGrid(things, entry, 60.0f);
            for (Creature* thing : things)
                if (TempSummon* thingSummon = thing->ToTempSummon())
                    if (thingSummon->GetSummonerGUID() == miner->GetGUID())
                        thing->DespawnOrUnsummon();
        }
        miner->DespawnOrUnsummon();
    }
}

struct npc_lost_isles_foreman_dampwick_mine : public ScriptedAI
{
    npc_lost_isles_foreman_dampwick_mine(Creature* creature) : ScriptedAI(creature) { }

    void sGossipSelect(Player* player, uint32 sender, uint32 action) override
    {
        HandleLostIslesGossipSpell(player, sender, action);
    }

    void sQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() == QUEST_CAPTURING_THE_UNKNOWN)
        {
            // The camera markers show only to those who are to photograph them
            player->CastSpell(player, SPELL_SEE_INVIS_AREA_MASTER, true);
            return;
        }

        if (quest->GetQuestId() != QUEST_MINER_TROUBLES)
            return;

        Talk(SAY_DAMPWICK_MINER_TROUBLES, player);
        player->CastSpell(player, SPELL_MINER_QUEST_ACCEPT, true);
    }

    void sQuestReward(Player* player, Quest const* quest, uint32 /*opt*/) override
    {
        if (quest->GetQuestId() != QUEST_MINER_TROUBLES)
            return;

        Talk(SAY_DAMPWICK_MINE_CLAIMED, player);
    }
};

// 68061 Miner Troubles: Quest Accept: a fresh start; its force cast then gives the controller, which brings the miner
class spell_lost_isles_miner_quest_accept : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_miner_quest_accept);

    void HandleBefore()
    {
        if (Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr)
            player->RemoveAurasDueToSpell(SPELL_FRIGHTENED_MINER_CONTROLLER);
    }

    void Register() override
    {
        BeforeCast += SpellCastFn(spell_lost_isles_miner_quest_accept::HandleBefore);
    }
};

// 68062 Miner Troubles: Frightened Miner Controller Aura: its end takes the miner away
class spell_lost_isles_frightened_miner_controller : public AuraScript
{
    PrepareAuraScript(spell_lost_isles_frightened_miner_controller);

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Player* player = GetTarget()->ToPlayer())
            DespawnFrightenedMiner(player);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_lost_isles_frightened_miner_controller::HandleRemove, EFFECT_0,
            SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 68060 Miner Troubles: Despawn Frightened Miner (from Quest Complete, 68063, the quest's reward spell)
class spell_lost_isles_despawn_frightened_miner : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_despawn_frightened_miner);

    void HandleAfter()
    {
        if (Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr)
        {
            player->RemoveAurasDueToSpell(SPELL_FRIGHTENED_MINER_CONTROLLER);
            DespawnFrightenedMiner(player);
        }
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_lost_isles_despawn_frightened_miner::HandleAfter);
    }
};

// 68069 Miner Troubles: New Miner from Gossip ("Er, Dampwick, I need another miner...."): a new miner from the start
class spell_lost_isles_new_miner_from_gossip : public AuraScript
{
    PrepareAuraScript(spell_lost_isles_new_miner_from_gossip);

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Player* player = GetTarget()->ToPlayer();
        if (!player)
            return;

        player->RemoveAurasDueToSpell(SPELL_FRIGHTENED_MINER_CONTROLLER);
        if (player->GetQuestStatus(QUEST_MINER_TROUBLES) == QUEST_STATUS_INCOMPLETE)
            player->CastSpell(player, SPELL_FRIGHTENED_MINER_CONTROLLER, true);
        Remove();
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_lost_isles_new_miner_from_gossip::HandleApply, EFFECT_0,
            SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

class player_lost_isles_miner_troubles : public PlayerScript
{
public:
    player_lost_isles_miner_troubles() : PlayerScript("player_lost_isles_miner_troubles",
        { PLAYERHOOK_ON_QUEST_ABANDON }) { }

    void OnPlayerQuestAbandon(Player* player, uint32 questId) override
    {
        if (questId == QUEST_MINER_TROUBLES)
        {
            player->RemoveAurasDueToSpell(SPELL_FRIGHTENED_MINER_CONTROLLER);
            DespawnFrightenedMiner(player);
        }
    }
};

/*######
## 35818 Chunk of Kaja'mite: the miner's find (68072) climbs into his ore cart (68074)
######*/

struct npc_lost_isles_kajamite_chunk : public ScriptedAI
{
    npc_lost_isles_kajamite_chunk(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        me->SetReactState(REACT_PASSIVE);
        DoCastSelf(SPELL_CHUNK_RIDE_ORE_CART, true);
    }

    void UpdateAI(uint32 /*diff*/) override { }
};

// 68074 Miner Troubles: Chunk of Kaja'mite Ride Ore Cart on Spawn: into a free seat of the cart of the miner who found it
class spell_lost_isles_chunk_ride_ore_cart : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_chunk_ride_ore_cart);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Creature* chunk = GetCaster() ? GetCaster()->ToCreature() : nullptr;
        TempSummon* summon = chunk ? chunk->ToTempSummon() : nullptr;
        if (!summon)
            return;

        std::list<Creature*> carts;
        chunk->GetCreatureListWithEntryInGrid(carts, NPC_ORE_CART, 30.0f);
        for (Creature* cart : carts)
        {
            TempSummon* cartSummon = cart->ToTempSummon();
            if (!cartSummon || cartSummon->GetSummonerGUID() != summon->GetSummonerGUID())
                continue;
            if (Vehicle* kit = cart->GetVehicleKit())
                if (kit->GetAvailableSeatCount())
                {
                    chunk->EnterVehicle(cart);
                    return;
                }
        }
        // a full cart: the chunk lies by it a while
        chunk->DespawnOrUnsummon(30s);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_lost_isles_chunk_ride_ore_cart::HandleScript, EFFECT_0,
            SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

/*######
## 37872, 37895, 37896, 37897 Capturing the Unknown (14031): the three cave paintings and the pygmy altar the KTC
## Snapflash photographs, by the quest's own spells. Each marker is invisible to all but those who are still to
## photograph it (Invis & State Visual 68231/70686/70687/70688 on the marker, the bobbing camera; See Invis Target Bunny
## 70661/70678/70680/70681 on the player, given by See Invis Area Master 68920 while the quest is under way for each one
## not yet captured). The Snapflash (68280) finds the marker near the player, which has them cast the photo (AICast to
## Player 68279 -> 68281); the player then sees the painting from the camera's spot, an ELM bunny facing it (Player's
## Bind Sight 70641, Player's Screen Effect 70649), the flash (KTC Snapflash Effect 68296) captures it and the marker
## takes that player's sight of it away (Remove See Invis 68349/68936/68943/68937).
######*/

enum CapturingTheUnknown
{
    EVENT_CAPTURE_FLASH             = 1,
    EVENT_CAPTURE_END,

    SPELL_KTC_SNAPFLASH             = 68281,
    SPELL_KTC_SNAPFLASH_EFFECT      = 68296,
    SPELL_CAPTURE_AICAST_TO_PLAYER  = 68279,
    SPELL_PLAYERS_BIND_SIGHT        = 70641,
    SPELL_PLAYERS_SCREEN_EFFECT     = 70649,
};

struct CapturingMarker
{
    uint32 Entry;
    uint32 SeeInvis;    // the player's sight of it
    uint32 RemoveSight; // the marker's spell that takes the sight away
};

std::array<CapturingMarker, 4> const CapturingMarkers =
{ {
    { NPC_CAPTURING_BUNNY_1, SPELL_SEE_INVIS_BUNNY_1, 68349 },
    { NPC_CAPTURING_BUNNY_2, SPELL_SEE_INVIS_BUNNY_2, 68936 },
    { NPC_CAPTURING_BUNNY_3, SPELL_SEE_INVIS_BUNNY_3, 68943 },
    { NPC_CAPTURING_BUNNY_4, SPELL_SEE_INVIS_BUNNY_4, 68937 },
} };

bool IsCapturingMarkerDone(Player* player, uint32 entry)
{
    Quest const* quest = sObjectMgr->GetQuestTemplate(QUEST_CAPTURING_THE_UNKNOWN);
    if (!quest)
        return true;

    uint16 const slot = player->FindQuestSlot(QUEST_CAPTURING_THE_UNKNOWN);
    for (uint8 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
        if (quest->RequiredNpcOrGo[i] == int32(entry))
            return slot >= MAX_QUEST_LOG_SIZE || player->GetQuestSlotCounter(slot, i) >= quest->RequiredNpcOrGoCount[i];
    return true;
}

// The player sees exactly the markers they are still to photograph: none outside the quest
void SyncCapturingSight(Player* player)
{
    bool const underWay = player->GetQuestStatus(QUEST_CAPTURING_THE_UNKNOWN) == QUEST_STATUS_INCOMPLETE;
    for (CapturingMarker const& marker : CapturingMarkers)
    {
        bool const see = underWay && !IsCapturingMarkerDone(player, marker.Entry);
        if (see && !player->HasAura(marker.SeeInvis))
            player->CastSpell(player, marker.SeeInvis, true);
        else if (!see)
            player->RemoveAurasDueToSpell(marker.SeeInvis);
    }
}

struct npc_lost_isles_capturing_bunny : public ScriptedAI
{
    npc_lost_isles_capturing_bunny(Creature* creature) : ScriptedAI(creature) { }

    void SpellHit(Unit* caster, SpellInfo const* spell) override
    {
        Player* player = caster ? caster->ToPlayer() : nullptr;
        if (!player || spell->Id != SPELL_KTC_SNAPFLASH || !_playerGUID.IsEmpty())
            return;

        // the photo: the player looks at the painting from the camera's spot
        _playerGUID = player->GetGUID();
        // Its own target is the nearest creature of an entry it is not told: the aura goes straight onto the camera
        // (the ELM bunny facing the painting), the player's, which moves their view there
        if (Creature* camera = me->FindNearestCreature(NPC_ELM_GENERAL_PURPOSE_BUNNY, 10.0f))
        {
            _cameraGUID = camera->GetGUID();
            player->AddAura(SPELL_PLAYERS_BIND_SIGHT, camera);
        }
        player->CastSpell(player, SPELL_PLAYERS_SCREEN_EFFECT, true);
        _events.ScheduleEvent(EVENT_CAPTURE_FLASH, 2s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID);
            switch (eventId)
            {
                case EVENT_CAPTURE_FLASH:
                    if (player)
                    {
                        // the flash, and it is captured: the marker leaves this player's sight
                        player->CastSpell(me, SPELL_KTC_SNAPFLASH_EFFECT, true);
                        player->KilledMonsterCredit(me->GetEntry());
                        for (CapturingMarker const& marker : CapturingMarkers)
                            if (marker.Entry == me->GetEntry())
                                DoCast(player, marker.RemoveSight, true);
                    }
                    _events.ScheduleEvent(EVENT_CAPTURE_END, 2s);
                    break;
                case EVENT_CAPTURE_END:
                    if (Creature* camera = ObjectAccessor::GetCreature(*me, _cameraGUID))
                        camera->RemoveAurasDueToSpell(SPELL_PLAYERS_BIND_SIGHT, _playerGUID);
                    if (player)
                    {
                        player->RemoveAurasDueToSpell(SPELL_PLAYERS_SCREEN_EFFECT);
                        SyncCapturingSight(player);
                    }
                    me->RemoveAurasDueToSpell(SPELL_KTC_SNAPFLASH);
                    _playerGUID.Clear();
                    _cameraGUID.Clear();
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
    ObjectGuid _cameraGUID;
};

// 68280 KTC Snapflash (the item's spell): it finds a marker near the player (conditions on the four), which turns to the
// player (68279) to have the photo taken, if that one is still to be captured
class spell_lost_isles_ktc_snapflash : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_ktc_snapflash);

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        Creature* marker = GetHitCreature();
        if (!player || !marker || player->GetQuestStatus(QUEST_CAPTURING_THE_UNKNOWN) != QUEST_STATUS_INCOMPLETE
            || IsCapturingMarkerDone(player, marker->GetEntry()))
            return;

        marker->CastSpell(player, SPELL_CAPTURE_AICAST_TO_PLAYER, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_lost_isles_ktc_snapflash::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 68279 Capturing The Unknown: AICast to Player: the player takes the photo of the marker that cast it (68281)
class spell_lost_isles_capturing_aicast : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_capturing_aicast);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetHitUnit() ? GetHitUnit()->ToPlayer() : nullptr;
        Unit* marker = GetCaster();
        if (player && marker)
            player->CastSpell(marker, SPELL_KTC_SNAPFLASH, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_lost_isles_capturing_aicast::HandleScript, EFFECT_0,
            SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 68920 Capturing The Unknown: See Invis Area Master: the sight of the markers still to be captured
class spell_lost_isles_capturing_sight : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_capturing_sight);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        if (Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr)
            SyncCapturingSight(player);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_lost_isles_capturing_sight::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 68349, 68936, 68943, 68937 Capturing The Unknown: Remove See Invis 1-4: the captured marker leaves the player's sight
class spell_lost_isles_capturing_remove_sight : public AuraScript
{
    PrepareAuraScript(spell_lost_isles_capturing_remove_sight);

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        for (CapturingMarker const& marker : CapturingMarkers)
            if (marker.RemoveSight == GetId())
                GetTarget()->RemoveAurasDueToSpell(marker.SeeInvis);
        Remove();
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_lost_isles_capturing_remove_sight::HandleApply, EFFECT_0,
            SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// The sight follows the quest: at login, when its objectives are done and when it is abandoned
class player_lost_isles_capturing_the_unknown : public PlayerScript
{
public:
    player_lost_isles_capturing_the_unknown() : PlayerScript("player_lost_isles_capturing_the_unknown",
        { PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_PLAYER_COMPLETE_QUEST, PLAYERHOOK_ON_QUEST_ABANDON }) { }

    void OnPlayerLogin(Player* player) override
    {
        SyncCapturingSight(player);
    }

    void OnPlayerCompleteQuest(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() == QUEST_CAPTURING_THE_UNKNOWN)
            SyncCapturingSight(player);
    }

    void OnPlayerQuestAbandon(Player* player, uint32 questId) override
    {
        if (questId != QUEST_CAPTURING_THE_UNKNOWN)
            return;

        for (CapturingMarker const& marker : CapturingMarkers)
            player->RemoveAurasDueToSpell(marker.SeeInvis);
    }
};

/*######
## 35838 Pygmy Witchdoctor: chants
######*/

enum PygmyWitchdoctor
{
    EVENT_WITCHDOCTOR_CHANT         = 1,
    SAY_WITCHDOCTOR_FIRST           = 0,
    SAY_WITCHDOCTOR_LAST            = 2,
};

struct npc_lost_isles_pygmy_witchdoctor : public ScriptedAI
{
    npc_lost_isles_pygmy_witchdoctor(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _events.ScheduleEvent(EVENT_WITCHDOCTOR_CHANT, 15s, 20s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (_events.ExecuteEvent() == EVENT_WITCHDOCTOR_CHANT)
        {
            if (Player* player = me->SelectNearestPlayer(25.0f))
                Talk(urand(SAY_WITCHDOCTOR_FIRST, SAY_WITCHDOCTOR_LAST), player);
            _events.ScheduleEvent(EVENT_WITCHDOCTOR_CHANT, 15s, 20s);
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
};

/*######
## The Vicious Vale's show fight: 35894 Orc Scout and 35893 Kilag Gorefang against 35896 Poison Spitter,
## 35897 Freezya and 35995 Strangle Vine. Neither side can hurt the other.
######*/

enum ValeFight
{
    EVENT_VALE_SHOW_FIGHT           = 1,
    EVENT_VALE_PLAYER_COOLDOWN,
    EVENT_PLANT_THORNS,
    EVENT_PLANT_POISON_SPIT,
    EVENT_PLANT_FROSTBOLT,

    SPELL_ORC_SHOOT                 = 15620,
    SPELL_POISON_SPIT               = 68207,
    SPELL_SHOOT_THORNS              = 68208,
    SPELL_FREEZYA_FROSTBOLT         = 68209,
    SPELL_PLANT_BURST               = 68226,
    SPELL_PLANT_BURST_CONTROLLER    = 68227,
    SPELL_WEED_WHACKER              = 68212,
    SPELL_WEED_WHACKER_BUFF         = 68824,

    SAY_KILAG_WEED_WHACKER          = 0,
    SAY_KILAG_BACK_TO_AGGRA         = 1,

    DATA_PLAYER_GUID                = 1,
};

bool IsValeOrc(uint32 entry)
{
    return entry == NPC_KILAG_GOREFANG || entry == NPC_ORC_SCOUT;
}

bool IsValePlant(uint32 entry)
{
    return entry == NPC_POISON_SPITTER || entry == NPC_FREEZYA || entry == NPC_STRANGLE_VINE;
}

struct npc_lost_isles_vale_orc : public ScriptedAI
{
    npc_lost_isles_vale_orc(Creature* creature) : ScriptedAI(creature) { }

    // The camp holds the vale against the plants: Kilag keeps his rifle up and shoots at them, his scouts stand by him
    // ready to fight; no one is hurt (the show goes on until The Vicious Vale is done)
    void Reset() override
    {
        _events.Reset();
        me->SetCombatMovement(false);
        me->SetReactState(REACT_PASSIVE);
        bool const kilag = me->GetEntry() == NPC_KILAG_GOREFANG;
        me->SetEmoteState(kilag ? EMOTE_STATE_READY_RIFLE : EMOTE_STATE_READY1H);
        if (kilag)
            _events.ScheduleEvent(EVENT_VALE_SHOW_FIGHT, 2s, 4s);
        else
            _events.ScheduleEvent(EVENT_VALE_SHOW_FIGHT, 20s, 60s);
    }

    void DamageTaken(Unit* attacker, uint32& damage, DamageEffectType /*type*/, SpellSchoolMask /*school*/) override
    {
        if (attacker && IsValePlant(attacker->GetEntry()))
            damage = 0;
    }

    void sGossipHello(Player* player) override
    {
        _playerGUID = player->GetGUID();
        if (player->HasAura(SPELL_WEED_WHACKER))
        {
            player->RemoveAurasDueToSpell(SPELL_WEED_WHACKER);
            player->RemoveAurasDueToSpell(SPELL_WEED_WHACKER_BUFF);
        }
    }

    void sQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() == QUEST_WEED_WHACKER)
            Talk(SAY_KILAG_WEED_WHACKER, player);
        else if (quest->GetQuestId() == QUEST_BACK_TO_AGGRA)
            Talk(SAY_KILAG_BACK_TO_AGGRA, player);
    }

    void JustSummoned(Creature* summon) override
    {
        if (summon->GetEntry() == NPC_ORC_SCOUT_LEAVING)
            summon->AI()->SetGUID(_playerGUID, DATA_PLAYER_GUID);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (_events.ExecuteEvent() == EVENT_VALE_SHOW_FIGHT)
        {
            bool const kilag = me->GetEntry() == NPC_KILAG_GOREFANG;
            if (Creature* target = SelectRandomCreatureInRange(me,
                { NPC_POISON_SPITTER, NPC_FREEZYA, NPC_STRANGLE_VINE }, kilag ? 30.0f : 25.0f))
            {
                me->SetFacingToObject(target);
                DoCast(target, SPELL_ORC_SHOOT, true);
            }
            me->SetEmoteState(kilag ? EMOTE_STATE_READY_RIFLE : EMOTE_STATE_READY1H);
            if (kilag)
                _events.ScheduleEvent(EVENT_VALE_SHOW_FIGHT, 2500ms, 4s);
            else
                _events.ScheduleEvent(EVENT_VALE_SHOW_FIGHT, 20s, 60s);
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
};

// 35896 Poison Spitter, 35897 Freezya: shoot at the orcs, and at players new to the vale or done with it
struct npc_lost_isles_vale_plant : public ScriptedAI
{
    npc_lost_isles_vale_plant(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _playerCooldown = false;
        _fighting = false;
        me->SetCombatMovement(false);
        me->SetReactState(REACT_PASSIVE);
        _events.ScheduleEvent(EVENT_VALE_SHOW_FIGHT, 20s, 120s);
    }

    void DamageTaken(Unit* attacker, uint32& damage, DamageEffectType /*type*/, SpellSchoolMask /*school*/) override
    {
        if (!attacker)
            return;

        if (IsValeOrc(attacker->GetEntry()))
        {
            damage = 0;
            return;
        }

        // A player's blow: it fights back, rooted, with its retail kit (ShatterCore's sniff of Weed Whacker)
        Player* player = attacker->GetCharmerOrOwnerPlayerOrPlayerItself();
        if (!player || _fighting)
            return;

        _fighting = true;
        me->SetReactState(REACT_AGGRESSIVE);
        AttackStart(player);
        if (me->GetEntry() == NPC_FREEZYA)
            _events.ScheduleEvent(EVENT_PLANT_FROSTBOLT, 1500ms, 3s);
        else
        {
            _events.ScheduleEvent(EVENT_PLANT_THORNS, 2s, 3500ms);
            _events.ScheduleEvent(EVENT_PLANT_POISON_SPIT, 5s, 9s);
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoCastSelf(SPELL_PLANT_BURST, true);
        DoCastSelf(SPELL_PLANT_BURST_CONTROLLER, true);
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (_playerCooldown)
            return;

        Player* player = who->ToPlayer();
        if (!player || !me->IsWithinDistInMap(player, 25.0f) || !me->IsValidAttackTarget(player))
            return;

        if (player->GetQuestStatus(QUEST_THE_VICIOUS_VALE) == QUEST_STATUS_NONE
            || player->GetQuestStatus(QUEST_BACK_TO_AGGRA) == QUEST_STATUS_REWARDED)
        {
            DoCast(player, RandomSpell());
            _playerCooldown = true;
            _events.ScheduleEvent(EVENT_VALE_PLAYER_COOLDOWN, 45s, 90s);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_VALE_SHOW_FIGHT:
                    if (Creature* target = SelectRandomCreatureInRange(me, { NPC_KILAG_GOREFANG, NPC_ORC_SCOUT },
                        25.0f))
                        DoCast(target, RandomSpell());
                    _events.ScheduleEvent(EVENT_VALE_SHOW_FIGHT, 40s, 180s);
                    break;
                case EVENT_VALE_PLAYER_COOLDOWN:
                    _playerCooldown = false;
                    break;
                case EVENT_PLANT_THORNS:
                    if (me->GetVictim())
                        DoCastVictim(SPELL_SHOOT_THORNS);
                    _events.ScheduleEvent(EVENT_PLANT_THORNS, 3s, 4500ms);
                    break;
                case EVENT_PLANT_POISON_SPIT:
                    if (me->GetVictim())
                        DoCastVictim(SPELL_POISON_SPIT);
                    _events.ScheduleEvent(EVENT_PLANT_POISON_SPIT, 10s, 14s);
                    break;
                case EVENT_PLANT_FROSTBOLT:
                    if (me->GetVictim())
                        DoCastVictim(SPELL_FREEZYA_FROSTBOLT);
                    _events.ScheduleEvent(EVENT_PLANT_FROSTBOLT, 2800ms, 4s);
                    break;
                default:
                    break;
            }
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    uint32 RandomSpell() const
    {
        if (me->GetEntry() == NPC_FREEZYA)
            return SPELL_FREEZYA_FROSTBOLT;
        return RAND(SPELL_POISON_SPIT, SPELL_SHOOT_THORNS);
    }

    EventMap _events;
    bool _playerCooldown = false;
    bool _fighting = false;
};

/*######
## 35995 Strangle Vine: holds an orc scout (36042, its vehicle accessory) until a player hits it
######*/

enum StrangleVine
{
    SPELL_EJECT_PASSENGER_1         = 60603,
    SAY_FREED_ORC_FIRST             = 0,
    SAY_FREED_ORC_LAST              = 4,
};

Position const FreedOrcRunPos = { 662.74f, 2844.35f, 85.899f, 0.0f };

struct npc_lost_isles_strangle_vine : public VehicleAI
{
    npc_lost_isles_strangle_vine(Creature* creature) : VehicleAI(creature) { }

    void Reset() override
    {
        VehicleAI::Reset();
        me->SetCombatMovement(false);
        me->SetReactState(REACT_PASSIVE);
    }

    void DamageTaken(Unit* attacker, uint32& /*damage*/, DamageEffectType /*type*/, SpellSchoolMask /*school*/) override
    {
        Player* player = attacker ? attacker->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
        if (!player)
            return;

        _playerGUID = player->GetGUID();
        if (Vehicle* vehicle = me->GetVehicleKit())
            if (vehicle->GetPassenger(0))
                DoCastSelf(SPELL_EJECT_PASSENGER_1, true);
    }

    void PassengerBoarded(Unit* passenger, int8 /*seatId*/, bool apply) override
    {
        if (apply)
            return;

        Creature* orc = passenger->ToCreature();
        if (!orc || orc->GetEntry() != NPC_ORC_SCOUT_FREED)
            return;

        if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
            orc->AI()->Talk(urand(SAY_FREED_ORC_FIRST, SAY_FREED_ORC_LAST), player);
        orc->GetMotionMaster()->MovePoint(0, FreedOrcRunPos);
        orc->DespawnOrUnsummon(8s);
    }

private:
    ObjectGuid _playerGUID;
};

/*######
## 35918 Orc Scout: Kilag's scouts leave for the overlook
######*/

enum OrcScoutLeaving
{
    EVENT_SCOUT_LEAVE               = 1,
    SAY_SCOUT_LEAVING_FIRST         = 0,
    SAY_SCOUT_LEAVING_LAST          = 1,
};

struct npc_lost_isles_orc_scout_leaving : public ScriptedAI
{
    npc_lost_isles_orc_scout_leaving(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _events.ScheduleEvent(EVENT_SCOUT_LEAVE, 1s);
    }

    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        if (id == DATA_PLAYER_GUID)
            _playerGUID = guid;
    }

    // Weed Whacker's reward spell (68289) is the player's own cast: the player summons them
    void IsSummonedBy(WorldObject* summoner) override
    {
        if (summoner && summoner->IsPlayer())
            _playerGUID = summoner->GetGUID();
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (_events.ExecuteEvent() == EVENT_SCOUT_LEAVE)
        {
            if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
                Talk(urand(SAY_SCOUT_LEAVING_FIRST, SAY_SCOUT_LEAVING_LAST), player);
            me->GetMotionMaster()->MovePoint(0, 661.02f, 2824.89f, 84.90f);
            me->DespawnOrUnsummon(10s);
        }
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
};

/*######
## 49108 Weed Whacker (14236): the quest's own spells, as the client defines them and ShatterCore wires them (Mr-Scooty,
## sniff). The item casts Weed Whacker (68211), which puts the whacker on the player (68212: vehicle 493, the mowing
## 68213 every half second, the speed 68824) and summons the Channel Bunny (68216), which rides the player (68217) and
## beams the whacker (68214). When the whacker comes off, the bunny goes (68215). Using the item again puts it away.
######*/

enum WeedWhacker
{
    SPELL_WEED_WHACKER_ITEM         = 68211,
    SPELL_WEED_WHACKER_BEAM         = 68214,
    SPELL_WEED_WHACKER_DESPAWN_BUNNY = 68215,
    SPELL_WEED_WHACKER_SUMMON_BUNNY = 68216,
    NPC_WEED_WHACKER_BUNNY          = 35903,
};

void DespawnWeedWhackerBunny(Unit* owner)
{
    std::list<Creature*> bunnies;
    owner->GetCreatureListWithEntryInGrid(bunnies, NPC_WEED_WHACKER_BUNNY, 30.0f);
    for (Creature* bunny : bunnies)
        if (TempSummon* summon = bunny->ToTempSummon())
            if (summon->GetSummonerGUID() == owner->GetGUID())
                bunny->DespawnOrUnsummon();
}

// 68211 Weed Whacker: the whacker first (the bunny boards the player, who must be a vehicle by then), then the bunny
class spell_lost_isles_weed_whacker : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_weed_whacker);

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (!caster || caster->HasAura(SPELL_WEED_WHACKER))
            return;

        caster->CastSpell(caster, SPELL_WEED_WHACKER, true);
        caster->CastSpell(caster, SPELL_WEED_WHACKER_SUMMON_BUNNY, true);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_lost_isles_weed_whacker::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 68212 Weed Whacker: the whacker off, the bunny and the speed go with it
class spell_lost_isles_weed_whacker_aura : public AuraScript
{
    PrepareAuraScript(spell_lost_isles_weed_whacker_aura);

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_WEED_WHACKER_DESPAWN_BUNNY, true);
        target->RemoveAurasDueToSpell(SPELL_WEED_WHACKER_BUFF);
        DespawnWeedWhackerBunny(target);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_lost_isles_weed_whacker_aura::HandleRemove, EFFECT_0,
            SPELL_AURA_SET_VEHICLE_ID, AURA_EFFECT_HANDLE_REAL);
    }
};

// 68215 Weed Whacker: Despawn Weed Whacker Channel Bunny
class spell_lost_isles_weed_whacker_despawn_bunny : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_weed_whacker_despawn_bunny);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        if (Unit* caster = GetCaster())
            DespawnWeedWhackerBunny(caster);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_lost_isles_weed_whacker_despawn_bunny::HandleScript, EFFECT_0,
            SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 35903 Weed Whacker Channel Bunny: rides the player and beams the whacker; gone with the whacker
struct npc_lost_isles_weed_whacker_bunny : public ScriptedAI
{
    npc_lost_isles_weed_whacker_bunny(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        Unit* owner = summoner ? summoner->ToUnit() : nullptr;
        if (!owner)
            return;

        _ownerGUID = owner->GetGUID();
        me->SetReactState(REACT_PASSIVE);
        _events.ScheduleEvent(2, 200ms);
        _events.ScheduleEvent(1, 2s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);
        while (uint32 eventId = _events.ExecuteEvent())
        {
            Unit* owner = ObjectAccessor::GetUnit(*me, _ownerGUID);
            if (!owner || !owner->HasAura(SPELL_WEED_WHACKER))
            {
                me->DespawnOrUnsummon();
                return;
            }

            if (eventId == 2)
            {
                // its ride (68217) targets the summoner as master, which a plain summon has not: it boards directly,
                // once the whacker has made the player a vehicle, and beams from there
                if (!me->GetVehicle() && owner->GetVehicleKit())
                {
                    me->EnterVehicle(owner, 0);
                    DoCast(owner, SPELL_WEED_WHACKER_BEAM, true);
                }
                if (!me->GetVehicle())
                    _events.ScheduleEvent(2, 300ms);
            }
            else
                _events.ScheduleEvent(1, 2s);
        }
    }

private:
    EventMap _events;
    ObjectGuid _ownerGUID;
};

class item_lost_isles_weed_whacker : public ItemScript
{
public:
    item_lost_isles_weed_whacker() : ItemScript("item_lost_isles_weed_whacker") { }

    bool OnUse(Player* player, Item* /*item*/, SpellCastTargets const& /*targets*/) override
    {
        if (!player->HasAura(SPELL_WEED_WHACKER))
            return false;

        player->RemoveAurasDueToSpell(SPELL_WEED_WHACKER);
        player->RemoveAurasDueToSpell(SPELL_WEED_WHACKER_BUFF);
        return true;
    }
};

/*######
## Stage 2 (phase 171): Infrared = Infradead and the ride to the cliffs
######*/

enum InfraredInfradead
{
    SPELL_INFRARED_HEAT_FOCALS      = 69303,
    SPELL_INFRARED_HEAT_FOCALS_2    = 68376,
    SPELL_ORC_SCOUT_CONTROLLER      = 68338,
    SPELL_INFRARED_QUEST_ACCEPT     = 68344,
    SPELL_DESPAWN_ORC_SCOUT         = 68337,
    SPELL_NEW_ORC_SCOUT_FROM_GOSSIP = 68346,
    SPELL_ORC_SCOUT_ON_DESPAWN      = 68371,
    SPELL_SCOUT_CHARGE              = 100,
    SPELL_SCOUT_REND                = 11977,
    SPELL_ASSASSIN_SINISTER_STRIKE  = 14873,
    SPELL_TO_THE_CLIFFS_SUMMON_BASTIA = 68973,

    SAY_KILAG_USE_FOCALS            = 0,
    SAY_KILAG_RIDE_BASTIA           = 1,
};

/*######
## 49611 Infrared Heat Focals: using them again takes them off; without a scout around, calls one
######*/

class item_lost_isles_infrared_heat_focals : public ItemScript
{
public:
    item_lost_isles_infrared_heat_focals() : ItemScript("item_lost_isles_infrared_heat_focals") { }

    bool OnUse(Player* player, Item* /*item*/, SpellCastTargets const& /*targets*/) override
    {
        if (player->HasAura(SPELL_INFRARED_HEAT_FOCALS))
        {
            player->RemoveAurasDueToSpell(SPELL_INFRARED_HEAT_FOCALS);
            player->RemoveAurasDueToSpell(SPELL_INFRARED_HEAT_FOCALS_2);
            return true;
        }

        if (!player->FindNearestCreature(NPC_ORC_SCOUT_INFRARED, 25.0f))
            player->CastSpell(player, SPELL_ORC_SCOUT_CONTROLLER, false);
        return false;
    }
};

/*######
## 35917 Kilag Gorefang (the overlook): Infrared = Infradead (14238), To the Cliffs (14240)
######*/

// The player's scout from the heat focals goes (handed in, abandoned, replaced)
void DespawnInfraredScout(Player* player)
{
    std::list<Creature*> scouts;
    player->GetCreatureListWithEntryInGrid(scouts, NPC_ORC_SCOUT_INFRARED, 200.0f);
    for (Creature* scout : scouts)
        if (scout->GetOwnerGUID() == player->GetGUID()
            || (scout->ToTempSummon() && scout->ToTempSummon()->GetSummonerGUID() == player->GetGUID()))
            scout->DespawnOrUnsummon();
}

struct npc_lost_isles_kilag_gorefang_overlook : public ScriptedAI
{
    npc_lost_isles_kilag_gorefang_overlook(Creature* creature) : ScriptedAI(creature) { }

    // "Kilag, I... lost my scout" (New Orc Scout from Gossip), "I need to borrow your tamed jungle cat" (Bastia)
    void sGossipSelect(Player* player, uint32 sender, uint32 action) override
    {
        HandleLostIslesGossipSpell(player, sender, action);
    }

    void sGossipHello(Player* player) override
    {
        if (player->HasAura(SPELL_INFRARED_HEAT_FOCALS)
            && player->GetQuestStatus(QUEST_INFRARED_INFRADEAD) != QUEST_STATUS_INCOMPLETE)
        {
            player->RemoveAurasDueToSpell(SPELL_INFRARED_HEAT_FOCALS);
            player->RemoveAurasDueToSpell(SPELL_INFRARED_HEAT_FOCALS_2);
        }
    }

    void sQuestAccept(Player* player, Quest const* quest) override
    {
        switch (quest->GetQuestId())
        {
            case QUEST_INFRARED_INFRADEAD:
                Talk(SAY_KILAG_USE_FOCALS, player);
                DoCast(player, SPELL_INFRARED_QUEST_ACCEPT);
                break;
            case QUEST_TO_THE_CLIFFS:
                Talk(SAY_KILAG_RIDE_BASTIA, player);
                player->CastSpell(player, SPELL_TO_THE_CLIFFS_SUMMON_BASTIA, false);
                break;
            default:
                break;
        }
    }

    void sQuestReward(Player* player, Quest const* quest, uint32 /*opt*/) override
    {
        if (quest->GetQuestId() != QUEST_INFRARED_INFRADEAD)
            return;

        player->CastSpell(player, SPELL_DESPAWN_ORC_SCOUT, false);
        player->RemoveAurasDueToSpell(SPELL_ORC_SCOUT_CONTROLLER);
        DespawnInfraredScout(player);
    }
};

// 68337 Infrared = Infradead: Despawn Orc Scout (from the quest's reward spell): the player's own scout goes
class spell_lost_isles_despawn_orc_scout : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_despawn_orc_scout);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        if (Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr)
            DespawnInfraredScout(player);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_lost_isles_despawn_orc_scout::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 68346 Infrared = Infradead: New Orc Scout from Gossip: the lost scout goes, a new one comes (the controller again)
class spell_lost_isles_new_orc_scout : public AuraScript
{
    PrepareAuraScript(spell_lost_isles_new_orc_scout);

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Player* player = GetTarget()->ToPlayer();
        if (!player)
            return;

        DespawnInfraredScout(player);
        player->RemoveAurasDueToSpell(SPELL_ORC_SCOUT_CONTROLLER);
        if (player->GetQuestStatus(QUEST_INFRARED_INFRADEAD) == QUEST_STATUS_INCOMPLETE)
            player->CastSpell(player, SPELL_ORC_SCOUT_CONTROLLER, true);
        Remove();
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_lost_isles_new_orc_scout::HandleApply, EFFECT_0, SPELL_AURA_DUMMY,
            AURA_EFFECT_HANDLE_REAL);
    }
};

class player_lost_isles_infrared : public PlayerScript
{
public:
    player_lost_isles_infrared() : PlayerScript("player_lost_isles_infrared", { PLAYERHOOK_ON_QUEST_ABANDON }) { }

    void OnPlayerQuestAbandon(Player* player, uint32 questId) override
    {
        if (questId != QUEST_INFRARED_INFRADEAD)
            return;

        player->RemoveAurasDueToSpell(SPELL_ORC_SCOUT_CONTROLLER);
        player->RemoveAurasDueToSpell(SPELL_INFRARED_HEAT_FOCALS);
        player->RemoveAurasDueToSpell(SPELL_INFRARED_HEAT_FOCALS_2);
        DespawnInfraredScout(player);
    }
};

/*######
## 36092 SI:7 Assassin: the hidden assassins of Infrared = Infradead fight with Sinister Strike (ShatterCore's sniff)
######*/

struct npc_lost_isles_si7_assassin : public ScriptedAI
{
    npc_lost_isles_si7_assassin(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _events.ScheduleEvent(1, 3s, 5s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _events.Update(diff);
        if (_events.ExecuteEvent())
        {
            DoCastVictim(SPELL_ASSASSIN_SINISTER_STRIKE);
            _events.ScheduleEvent(1, 5s, 8s);
        }

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
};

/*######
## 36100 Orc Scout: the player's escort while hunting with the heat focals
######*/

enum OrcScoutInfrared
{
    EVENT_INFRARED_SCOUT_TALK       = 1,
    EVENT_INFRARED_SCOUT_REND,
    SAY_INFRARED_SCOUT_GOGGLES      = 0,
};

struct npc_lost_isles_orc_scout_infrared : public ScriptedAI
{
    npc_lost_isles_orc_scout_infrared(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        if (!_playerGUID.IsEmpty())
            return;

        _playerGUID = me->GetOwnerGUID();
        _events.ScheduleEvent(EVENT_INFRARED_SCOUT_TALK, 1s);
    }

    void JustEngagedWith(Unit* who) override
    {
        DoCast(who, SPELL_SCOUT_CHARGE);
        _events.ScheduleEvent(EVENT_INFRARED_SCOUT_REND, 4s, 6s);
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoCastSelf(SPELL_ORC_SCOUT_ON_DESPAWN, true);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_INFRARED_SCOUT_TALK:
                    if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
                        Talk(SAY_INFRARED_SCOUT_GOGGLES, player);
                    break;
                case EVENT_INFRARED_SCOUT_REND:
                    if (me->GetVictim())
                        DoCastVictim(SPELL_SCOUT_REND);
                    _events.ScheduleEvent(EVENT_INFRARED_SCOUT_REND, 12s, 16s);
                    break;
                default:
                    break;
            }
        }

        // the player's fight is the scout's
        if (!me->GetVictim())
            if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
                if (Unit* enemy = player->GetVictim())
                    if (me->IsValidAttackTarget(enemy))
                        AttackStart(enemy);

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
};

// A flight that keeps clear of the ground: up, across and down. Ends with an EFFECT_MOTION_TYPE movement inform.
void MoveFlight(Creature* flyer, uint32 pointId, Position const& destination, float height)
{
    std::array<Position, 3> path =
    {
        Position(flyer->GetPositionX(), flyer->GetPositionY(), flyer->GetPositionZ() + height),
        Position(destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ() + height),
        destination,
    };
    flyer->GetMotionMaster()->MoveSmoothPath(pointId, path.data(), path.size(), false, true);
}

// A ride that ends at a point: path finding on the ground, a flight in the air. A ground path that stops short (mmaps
// cuts long paths) is continued by the caller from where it ended.
void MoveRide(Creature* vehicle, uint32 pointId, Position const& destination, bool flying)
{
    if (flying)
        MoveFlight(vehicle, pointId, destination, 30.0f);
    else
        vehicle->GetMotionMaster()->MovePoint(pointId, destination, FORCED_MOVEMENT_NONE, 0.0f, true, true);
}

bool IsRideEnd(uint32 type)
{
    return type == POINT_MOTION_TYPE || type == EFFECT_MOTION_TYPE;
}

/*######
## 36585 Bastia: carries the player from the overlook to Scout Brax (To the Cliffs, 14240)
######*/

enum Bastia
{
    EVENT_BASTIA_START              = 1,
    POINT_BASTIA_BRAX               = 1,
};

// The trail from the overlook up the cliffs to Scout Brax (36112): ArkCORE-NG's route for her (its path 3658500, run),
// which keeps to the path where path finding cut across the rocks
std::array<Position, 28> const BastiaTrail =
{ {
    { 868.2f, 2844.5f, 102.8f }, { 868.8f, 2865.4f, 101.1f }, { 874.0f, 2871.5f, 100.7f }, { 903.2f, 2877.1f, 100.2f },
    { 915.7f, 2889.1f, 100.0f }, { 924.3f, 2907.2f, 101.9f }, { 940.6f, 2916.7f, 105.1f }, { 947.8f, 2940.4f, 109.0f },
    { 956.5f, 2946.7f, 109.8f }, { 1035.3f, 2949.7f, 110.2f }, { 1044.2f, 2959.5f, 111.3f }, { 1049.5f, 2977.7f, 112.6f },
    { 1075.6f, 2999.2f, 117.4f }, { 1075.4f, 3025.4f, 121.8f }, { 1089.0f, 3040.2f, 123.1f }, { 1087.6f, 3063.5f, 123.8f },
    { 1070.1f, 3100.8f, 125.2f }, { 1048.2f, 3118.5f, 125.3f }, { 1031.4f, 3124.1f, 124.9f }, { 1025.0f, 3130.9f, 124.7f },
    { 1023.3f, 3149.5f, 122.9f }, { 1024.1f, 3166.8f, 119.8f }, { 1045.2f, 3182.3f, 116.7f }, { 1055.8f, 3197.6f, 113.2f },
    { 1058.0f, 3207.8f, 108.4f }, { 1060.1f, 3226.0f, 96.4f }, { 1075.5f, 3234.8f, 84.4f }, { 1081.0f, 3238.7f, 82.2f },
} };

struct npc_lost_isles_bastia : public VehicleAI
{
    npc_lost_isles_bastia(Creature* creature) : VehicleAI(creature) { }

    void PassengerBoarded(Unit* passenger, int8 /*seatId*/, bool apply) override
    {
        if (!passenger->IsPlayer())
            return;

        if (apply)
            _events.ScheduleEvent(EVENT_BASTIA_START, 3s);
        else
            me->DespawnOrUnsummon(1s);
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (!IsRideEnd(type) || id != POINT_BASTIA_BRAX)
            return;

        if (Vehicle* vehicle = me->GetVehicleKit())
            vehicle->RemoveAllPassengers();
        me->DespawnOrUnsummon(1s);
    }

    void UpdateAI(uint32 diff) override
    {
        VehicleAI::UpdateAI(diff);
        _events.Update(diff);

        if (_events.ExecuteEvent() == EVENT_BASTIA_START)
        {
            me->SetWalk(false);
            me->GetMotionMaster()->MoveSmoothPath(POINT_BASTIA_BRAX, BastiaTrail.data(), BastiaTrail.size(), false);
        }
    }

private:
    EventMap _events;
};

/*######
## Stage 3 (phase 172): the gyrochoppa and the precious cargo
######*/

enum PreciousCargo
{
    SOUND_PRECIOUS_CARGO_ACCEPT     = 16422,
    SOUND_PRECIOUS_CARGO_REWARD     = 16423,
    SOUND_WARCHIEFS_REVENGE_ACCEPT  = 16424,
    SPELL_PRECIOUS_CARGO_ACCEPT     = 68386, // summons the gyrochoppa 36143 and seats the player
    SPELL_EJECT_ALL_PASSENGERS      = 68576,
    SPELL_UP_UP_KILL_CREDIT         = 66127,
    SPELL_MEET_ME_UP_TOP_CONTROLLER = 68421,
    SPELL_SUMMON_THE_WARCHIEF       = 69079,

    EVENT_GYROCHOPPA_START          = 1,
    EVENT_THRALL_SUMMON_WARCHIEF    = 1,

    POINT_GYROCHOPPA_CARGO          = 1,
    SAY_GYROCHOPPA_FIND_CARGO       = 0,
};

/*######
## 36127 Gyrochoppa: Precious Cargo (14242) flies the player to the Alliance fleet
######*/

struct npc_lost_isles_gyrochoppa_quest : public ScriptedAI
{
    npc_lost_isles_gyrochoppa_quest(Creature* creature) : ScriptedAI(creature) { }

    void sQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() != QUEST_PRECIOUS_CARGO)
            return;

        player->PlayDirectSound(SOUND_PRECIOUS_CARGO_ACCEPT, player);
        player->CastSpell(player, SPELL_PRECIOUS_CARGO_ACCEPT, true);
    }
};

/*######
## 36143 Gyrochoppa: the flight to the ship that holds Thrall
######*/

// The flight from the sniff, as ShatterCore has it (Mr-Scooty): out over the bay and down to the water by Thrall's ship,
// where "Find the Precious Cargo!" and the player drops in; the rope ladders lead aboard
std::array<Position, 6> const GyrochoppaPath =
{ {
    { 846.74f, 3335.54f, 10.14f },
    { 755.89f, 3374.22f, 14.39f },
    { 780.33f, 3449.16f, 14.39f },
    { 1071.62f, 3589.42f, 26.22f },
    { 1098.78f, 3729.42f, 93.03f },
    { 971.51f, 3802.34f, 14.36f },
} };

struct npc_lost_isles_gyrochoppa : public VehicleAI
{
    npc_lost_isles_gyrochoppa(Creature* creature) : VehicleAI(creature) { }

    void PassengerBoarded(Unit* passenger, int8 /*seatId*/, bool apply) override
    {
        Player* player = passenger->ToPlayer();
        if (!player)
            return;

        if (apply)
            _events.ScheduleEvent(EVENT_GYROCHOPPA_START, 2s);
        else
        {
            _events.Reset();
            me->DespawnOrUnsummon(2s);
        }
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (!IsRideEnd(type) || id != POINT_GYROCHOPPA_CARGO)
            return;

        if (Vehicle* vehicle = me->GetVehicleKit())
            if (Unit* passenger = vehicle->GetPassenger(0))
                if (Player* player = passenger->ToPlayer())
                    Talk(SAY_GYROCHOPPA_FIND_CARGO, player);
        DoCastSelf(SPELL_EJECT_ALL_PASSENGERS, true);
        me->DespawnOrUnsummon(1s);
    }

    void UpdateAI(uint32 diff) override
    {
        VehicleAI::UpdateAI(diff);
        _events.Update(diff);

        if (_events.ExecuteEvent() == EVENT_GYROCHOPPA_START)
        {
            me->SetImmuneToAll(true);
            me->SetSpeed(MOVE_FLIGHT, 6.43f); // the sniff: 45 yards a second
            me->SetCanFly(true);
            me->SetDisableGravity(true);
            me->GetMotionMaster()->MoveSmoothPath(POINT_GYROCHOPPA_CARGO, GyrochoppaPath.data(), GyrochoppaPath.size(),
                false, true);
        }
    }

private:
    EventMap _events;
};

/*######
## 36145 Thrall (caged): talking to him opens the cage; Meet Me Up Top (14326) lets him out
######*/

Position const WarchiefSummonPos = { 993.1458f, 3852.019f, 3.2269f, 0.0f };

struct npc_lost_isles_thrall_caged : public ScriptedAI
{
    npc_lost_isles_thrall_caged(Creature* creature) : ScriptedAI(creature) { }

    void sGossipHello(Player* player) override
    {
        if (player->GetQuestStatus(QUEST_PRECIOUS_CARGO) != QUEST_STATUS_INCOMPLETE)
            return;

        player->KilledMonsterCredit(me->GetEntry());
        if (GameObject* cage = me->FindNearestGameObject(GO_ARCANE_CAGE, 10.0f))
        {
            cage->UseDoorOrButton();
            CloseGossipMenuFor(player);
        }
    }

    void sQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() != QUEST_MEET_ME_UP_TOP)
            return;

        player->CastSpell(player, SPELL_MEET_ME_UP_TOP_CONTROLLER, true);
        _playerGUID = player->GetGUID();
        _events.ScheduleEvent(EVENT_THRALL_SUMMON_WARCHIEF, 100ms);
    }

    void sQuestReward(Player* player, Quest const* quest, uint32 /*opt*/) override
    {
        if (quest->GetQuestId() == QUEST_PRECIOUS_CARGO)
            player->PlayDirectSound(SOUND_PRECIOUS_CARGO_REWARD, player);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (_events.ExecuteEvent() == EVENT_THRALL_SUMMON_WARCHIEF)
            if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
                player->CastSpell(WarchiefSummonPos.GetPositionX(), WarchiefSummonPos.GetPositionY(),
                    WarchiefSummonPos.GetPositionZ(), SPELL_SUMMON_THE_WARCHIEF, true);

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
};

/*######
## Stage 4 (phase 179): Warchief's Revenge, the camp on the cliffs and Up, Up & Away!
######*/

/*######
## 36622 Thrall: freed from the cage, goes up to the deck
######*/

enum ThrallFreed
{
    EVENT_THRALL_FREED_WALK         = 1,
    POINT_THRALL_UP_TOP             = 1,
    SAY_THRALL_SEE_YOU_UP_TOP       = 0,
};

struct npc_lost_isles_thrall_freed : public ScriptedAI
{
    npc_lost_isles_thrall_freed(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        me->SetImmuneToAll(true);
        _events.ScheduleEvent(EVENT_THRALL_FREED_WALK, 2s);
    }

    void IsSummonedBy(WorldObject* summoner) override
    {
        if (Player* player = summoner ? summoner->ToPlayer() : nullptr)
            _playerGUID = player->GetGUID();
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type == POINT_MOTION_TYPE && id == POINT_THRALL_UP_TOP)
            me->DespawnOrUnsummon(100ms);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (_events.ExecuteEvent() == EVENT_THRALL_FREED_WALK)
        {
            if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
                Talk(SAY_THRALL_SEE_YOU_UP_TOP, player);
            // Where Thrall waits up top in the next stage (36161)
            me->GetMotionMaster()->MovePoint(POINT_THRALL_UP_TOP, 980.9f, 3823.2f, 11.3f);
            me->DespawnOrUnsummon(30s);
        }
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
};

/*######
## 36161 Thrall (up top): Warchief's Revenge (14243) calls the Cyclone of the Elements; calls on the sea
######*/

enum ThrallUpTop
{
    EVENT_THRALL_CALL_1             = 1,
    EVENT_THRALL_CALL_2,
    EVENT_THRALL_CALL_COOLDOWN,

    EVENT_THRALL_STORM,

    SPELL_WARCHIEFS_REVENGE_ACCEPT  = 68408, // summons the cyclone 36178
    SPELL_THRALL_CHAIN_LIGHTNING    = 68440,

    SAY_THRALL_SPIRITS_OF_THE_SEA   = 0,
    SAY_THRALL_CHILDREN_OF_THE_DEPTHS = 1,
    SAY_THRALL_STORM_FIRST          = 2,
    SAY_THRALL_STORM_LAST           = 6,
};

struct npc_lost_isles_thrall_up_top : public ScriptedAI
{
    npc_lost_isles_thrall_up_top(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _callCooldown = false;
        _events.ScheduleEvent(EVENT_THRALL_STORM, 5s);
    }

    void sQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() != QUEST_WARCHIEFS_REVENGE)
            return;

        me->PlayDirectSound(SOUND_WARCHIEFS_REVENGE_ACCEPT, player);
        player->CastSpell(player, SPELL_WARCHIEFS_REVENGE_ACCEPT, true);
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (_callCooldown)
            return;

        Player* player = who->ToPlayer();
        if (!player || player->GetQuestStatus(QUEST_MEET_ME_UP_TOP) != QUEST_STATUS_COMPLETE)
            return;

        _callCooldown = true;
        _playerGUID = player->GetGUID();
        _events.ScheduleEvent(EVENT_THRALL_CALL_COOLDOWN, 90s);
        _events.ScheduleEvent(EVENT_THRALL_CALL_1, 100ms);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_THRALL_CALL_1:
                    if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
                        Talk(SAY_THRALL_SPIRITS_OF_THE_SEA, player);
                    _events.ScheduleEvent(EVENT_THRALL_CALL_2, 8s);
                    break;
                case EVENT_THRALL_CALL_2:
                    if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
                        Talk(SAY_THRALL_CHILDREN_OF_THE_DEPTHS, player);
                    break;
                case EVENT_THRALL_CALL_COOLDOWN:
                    _callCooldown = false;
                    break;
                case EVENT_THRALL_STORM:
                {
                    // While someone rides the cyclone, he strikes the sailors (harmless: they are the player's to kill)
                    Player* rider = nullptr;
                    for (auto const& ref : me->GetMap()->GetPlayers())
                        if (Player* player = ref.GetSource())
                            if (player->IsWithinDistInMap(me, 150.0f) && player->InSamePhase(me)
                                && player->GetQuestStatus(QUEST_WARCHIEFS_REVENGE) == QUEST_STATUS_INCOMPLETE)
                                rider = player;
                    if (rider)
                    {
                        if (Creature* sailor = me->FindNearestCreature(NPC_ALLIANCE_SAILOR, 60.0f))
                            DoCast(sailor, SPELL_THRALL_CHAIN_LIGHTNING, true);
                        if (roll_chance_i(35))
                            Talk(urand(SAY_THRALL_STORM_FIRST, SAY_THRALL_STORM_LAST), rider);
                    }
                    _events.ScheduleEvent(EVENT_THRALL_STORM, 6s, 10s);
                    break;
                }
                default:
                    break;
            }
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
    bool _callCooldown = false;
};

/*######
## 36177 Invisible Stalker (Quest Phase 4): now and then a fire on the burning ships
######*/

enum BurningShips
{
    EVENT_STALKER_FIRE              = 1,
    SPELL_COSMETIC_FLAME_PATCH      = 42345,
};

struct npc_lost_isles_ship_fire_stalker : public ScriptedAI
{
    npc_lost_isles_ship_fire_stalker(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _events.ScheduleEvent(EVENT_STALKER_FIRE, 1s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (_events.ExecuteEvent() == EVENT_STALKER_FIRE)
        {
            if (roll_chance_i(15))
                DoCastSelf(SPELL_COSMETIC_FLAME_PATCH);
            _events.ScheduleEvent(EVENT_STALKER_FIRE, 80s, 100s);
        }
    }

private:
    EventMap _events;
};

/*######
## 36176 Alliance Sailor: a sailor that fell below his deck goes back up
######*/

enum AllianceSailor
{
    EVENT_SAILOR_CHECK_DECK         = 1,
};

struct npc_lost_isles_alliance_sailor : public ScriptedAI
{
    npc_lost_isles_alliance_sailor(Creature* creature) : ScriptedAI(creature) { }

    void DamageTaken(Unit* attacker, uint32& damage, DamageEffectType /*type*/, SpellSchoolMask /*school*/) override
    {
        if (attacker && attacker->GetEntry() == NPC_THRALL_UP_TOP)
            damage = 0;
    }

    void Reset() override
    {
        _events.Reset();
        _events.ScheduleEvent(EVENT_SAILOR_CHECK_DECK, 2s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (_events.ExecuteEvent() == EVENT_SAILOR_CHECK_DECK && !me->IsInCombat())
            if (me->GetPositionZ() < me->GetHomePosition().GetPositionZ() + 0.2f)
                me->GetMotionMaster()->MoveTargetedHome();

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
};

/*######
## 36178 Cyclone of the Elements: circles the Alliance fleet until fifty sailors are dead, then flies to Thrall's camp
######*/

enum CycloneOfTheElements
{
    EVENT_CYCLONE_START             = 1,
    EVENT_CYCLONE_CHECK_QUEST,

    SPELL_CYCLONE_RIDE              = 68436,
    SPELL_TO_THE_WILD_OVERLOOK      = 68409,
    SPELL_WARCHIEFS_REVENGE_ABANDON = 68438,
    SPELL_WARCHIEFS_REVENGE_GOAL    = 68439,
    POINT_CYCLONE_CAMP              = 1,
    POINT_CYCLONE_LIFTOFF,
    POINT_CYCLONE_CIRCUIT,
    ALLIANCE_SAILORS_TO_KILL        = 50,
};

// The ride from the sniff, as ShatterCore has it (Mr-Scooty): off the Vengeance Wake, round and round a circuit over both
// ships while the player strikes the sailors, then, the fiftieth fallen, to the Wild Overlook by Thrall's camp
std::array<Position, 3> const CycloneLiftOff =
{ {
    { 984.95f, 3826.14f, 10.03f }, { 995.30f, 3830.47f, 11.08f }, { 1005.22f, 3832.42f, 14.53f },
} };
std::array<Position, 22> const CycloneCircuit =
{ {
    { 1042.18f, 3857.24f, 23.60f }, { 1071.38f, 3852.88f, 21.19f }, { 1097.26f, 3826.48f, 18.91f },
    { 1097.90f, 3780.77f, 20.30f }, { 1062.80f, 3755.34f, 22.85f }, { 1033.11f, 3773.80f, 23.60f },
    { 1021.79f, 3827.31f, 20.99f }, { 1004.84f, 3861.21f, 19.96f }, { 962.51f, 3859.35f, 20.30f },
    { 937.91f, 3841.93f, 19.27f }, { 903.89f, 3827.22f, 19.57f }, { 867.18f, 3849.85f, 21.32f },
    { 866.32f, 3895.80f, 21.52f }, { 893.65f, 3935.71f, 20.88f }, { 936.51f, 3944.06f, 21.13f },
    { 968.71f, 3914.84f, 20.38f }, { 952.70f, 3861.26f, 24.55f }, { 936.18f, 3820.98f, 21.94f },
    { 943.95f, 3785.92f, 22.27f }, { 975.88f, 3771.54f, 20.71f }, { 1004.32f, 3782.39f, 21.71f },
    { 1022.35f, 3831.35f, 24.63f },
} };
std::array<Position, 9> const CycloneDeparture =
{ {
    { 956.42f, 3740.38f, 70.96f }, { 950.88f, 3662.27f, 14.80f }, { 915.74f, 3451.17f, 2.30f },
    { 907.14f, 3327.42f, 23.49f }, { 960.32f, 3212.72f, 111.31f }, { 995.90f, 3143.98f, 145.03f },
    { 1007.96f, 3070.12f, 147.64f }, { 901.75f, 2926.98f, 147.64f }, { 872.49f, 2765.59f, 119.89f },
} };

struct npc_lost_isles_cyclone_of_the_elements : public VehicleAI
{
    npc_lost_isles_cyclone_of_the_elements(Creature* creature) : VehicleAI(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        Player* player = summoner ? summoner->ToPlayer() : nullptr;
        if (!player)
            return;

        _playerGUID = player->GetGUID();
        me->SetImmuneToAll(true);
        me->SetCanFly(true);
        me->SetDisableGravity(true);
        if (!player->GetVehicle())
            player->CastSpell(me, SPELL_CYCLONE_RIDE, true);
    }

    void PassengerBoarded(Unit* passenger, int8 /*seatId*/, bool apply) override
    {
        Player* player = passenger->ToPlayer();
        if (!player)
            return;

        if (apply)
            _events.ScheduleEvent(EVENT_CYCLONE_START, 1s);
        else
        {
            player->RemoveAurasDueToSpell(SPELL_CYCLONE_RIDE);
            player->RemoveAurasDueToSpell(SPELL_MEET_ME_UP_TOP_CONTROLLER);
            me->DespawnOrUnsummon(1s);
        }
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (!IsRideEnd(type))
            return;

        switch (id)
        {
            case POINT_CYCLONE_LIFTOFF:
            case POINT_CYCLONE_CIRCUIT:
                // round the circuit until the goal is reached
                if (!_departing)
                    me->GetMotionMaster()->MoveSmoothPath(POINT_CYCLONE_CIRCUIT, CycloneCircuit.data(),
                        CycloneCircuit.size(), false, true);
                break;
            case POINT_CYCLONE_CAMP:
                if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
                    player->RemoveAurasDueToSpell(SPELL_TO_THE_WILD_OVERLOOK);
                if (Vehicle* vehicle = me->GetVehicleKit())
                    vehicle->RemoveAllPassengers();
                me->DespawnOrUnsummon(1s);
                break;
            default:
                break;
        }
    }

    void UpdateAI(uint32 diff) override
    {
        VehicleAI::UpdateAI(diff);
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
            case EVENT_CYCLONE_START:
                    me->SetSpeed(MOVE_FLIGHT, 1.63f); // the sniff: 11.4 yards a second round the ships
                    me->GetMotionMaster()->MoveSmoothPath(POINT_CYCLONE_LIFTOFF, CycloneLiftOff.data(),
                        CycloneLiftOff.size(), false, true);
                    _events.ScheduleEvent(EVENT_CYCLONE_CHECK_QUEST, 1s);
                    break;
                case EVENT_CYCLONE_CHECK_QUEST:
                {
                    // ArkCORE checked in KilledUnit; the player's spells kill the sailors, so the count is read here
                    Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID);
                    if (player && player->GetQuestStatus(QUEST_WARCHIEFS_REVENGE) == QUEST_STATUS_INCOMPLETE
                        && player->GetReqKillOrCastCurrentCount(QUEST_WARCHIEFS_REVENGE, NPC_ALLIANCE_SAILOR)
                            < ALLIANCE_SAILORS_TO_KILL)
                    {
                        _events.ScheduleEvent(EVENT_CYCLONE_CHECK_QUEST, 1s);
                        break;
                    }
                    // the goal reached (or the quest given up): to the Wild Overlook
                    _departing = true;
                    if (player)
                    {
                        if (player->GetQuestStatus(QUEST_WARCHIEFS_REVENGE) == QUEST_STATUS_COMPLETE)
                            player->CastSpell(me, SPELL_WARCHIEFS_REVENGE_GOAL, true);
                        else
                            player->CastSpell(me, SPELL_WARCHIEFS_REVENGE_ABANDON, true);
                        player->CastSpell(player, SPELL_TO_THE_WILD_OVERLOOK, true);
                    }
                    me->GetMotionMaster()->Clear();
                    me->SetSpeed(MOVE_FLIGHT, 4.65f); // the sniff: 32.6 yards a second
                    me->GetMotionMaster()->MoveSmoothPath(POINT_CYCLONE_CAMP, CycloneDeparture.data(),
                        CycloneDeparture.size(), false, true);
                    break;
                }
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
    bool _departing = false;
};

/*######
## 23837 ELM General Purpose Bunny (Lost Isles spawns only, bound per spawn): the camp's sound, the turtle
## channelers of the volcano and the zombie spawner of the Three Little Pygmies' cave
######*/

enum ElmBunny
{
    EVENT_ELM_PERIODIC_SOUND        = 1,
    EVENT_ELM_TURTLE_CHANNEL,
    EVENT_ELM_SPAWN_ZOMBIE,

    SOUND_CAMP_PERIODIC             = 16459,
    SPELL_TURTLE_POWER_CHANNEL      = 72382,
    AREA_VOLCANO_TEMPLE             = 4886,
    AREA_ZOMBIE_CAVE                = 4899,
    MAX_CAVE_ZOMBIES                = 4,
};

struct npc_lost_isles_elm_bunny : public ScriptedAI
{
    npc_lost_isles_elm_bunny(Creature* creature) : ScriptedAI(creature), _summons(creature) { }

    void Reset() override
    {
        _events.Reset();
        if (std::abs(me->GetPositionX() - 876.2f) < 5.0f && std::abs(me->GetPositionY() - 2747.1f) < 5.0f)
            _events.ScheduleEvent(EVENT_ELM_PERIODIC_SOUND, 7250ms);
        else if (me->GetAreaId() == AREA_VOLCANO_TEMPLE)
            _events.ScheduleEvent(EVENT_ELM_TURTLE_CHANNEL, 1s);
        else if (me->GetAreaId() == AREA_ZOMBIE_CAVE)
            _events.ScheduleEvent(EVENT_ELM_SPAWN_ZOMBIE, 1s);
    }

    void JustSummoned(Creature* summon) override
    {
        _summons.Summon(summon);
    }

    void SummonedCreatureDespawn(Creature* summon) override
    {
        _summons.Despawn(summon);
    }

    void SummonedCreatureDies(Creature* summon, Unit* /*killer*/) override
    {
        _summons.Despawn(summon);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_ELM_PERIODIC_SOUND:
                    me->PlayDistanceSound(SOUND_CAMP_PERIODIC); // positional: a direct sound reached everyone in view range
                    _events.ScheduleEvent(EVENT_ELM_PERIODIC_SOUND, 7250ms);
                    break;
                case EVENT_ELM_TURTLE_CHANNEL:
                    DoCastSelf(SPELL_TURTLE_POWER_CHANNEL, true);
                    _events.ScheduleEvent(EVENT_ELM_TURTLE_CHANNEL, 15500ms);
                    break;
                case EVENT_ELM_SPAWN_ZOMBIE:
                    if (_summons.size() < MAX_CAVE_ZOMBIES)
                        me->SummonCreature(RAND(NPC_GOBLIN_ZOMBIE_NORD, NPC_GOBLIN_ZOMBIE_OST),
                            frand(1471.4f, 1479.4f), frand(1971.5f, 1979.5f), 238.076904f, 1.490936f,
                            TEMPSUMMON_TIMED_DESPAWN, 3 * MINUTE * IN_MILLISECONDS);
                    _events.ScheduleEvent(EVENT_ELM_SPAWN_ZOMBIE, 10s, 15s);
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
    SummonList _summons;
};

/*######
## 36525 Warrior-Matic NX-01 (the camp): beeps and works
######*/

enum WarriorMatic
{
    EVENT_WARRIOR_MATIC_BEEP        = 1,
    SOUND_WARRIOR_MATIC_BEEP        = 2304,
};

struct npc_lost_isles_warrior_matic_camp : public ScriptedAI
{
    npc_lost_isles_warrior_matic_camp(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _events.ScheduleEvent(EVENT_WARRIOR_MATIC_BEEP, 30s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (_events.ExecuteEvent() == EVENT_WARRIOR_MATIC_BEEP)
        {
            me->PlayDistanceSound(SOUND_WARRIOR_MATIC_BEEP);
            me->HandleEmoteCommand(EMOTE_ONESHOT_WOUND);
            _events.ScheduleEvent(EVENT_WARRIOR_MATIC_BEEP, 30s);
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
};

/*######
## 36425 Sassy Hardwrench (the camp): Up, Up & Away! (14244) brings the Trade Prince
######*/

enum SassyCamp
{
    SAY_SASSY_TRADE_PRINCE          = 0,
    SPELL_UP_UP_QUEST_ACCEPT        = 68815, // summons Trade Prince Gallywix 36513
};

struct npc_lost_isles_sassy_hardwrench_camp : public ScriptedAI
{
    npc_lost_isles_sassy_hardwrench_camp(Creature* creature) : ScriptedAI(creature) { }

    void sQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() != QUEST_UP_UP_AND_AWAY)
            return;

        Talk(SAY_SASSY_TRADE_PRINCE, player);
        player->CastSpell(player, SPELL_UP_UP_QUEST_ACCEPT, true);
    }
};

/*######
## 36421 Orc Survivor: talks with the other survivors
######*/

enum OrcSurvivor
{
    EVENT_ORC_SURVIVOR_TALK         = 1,
};

struct npc_lost_isles_orc_survivor : public ScriptedAI
{
    npc_lost_isles_orc_survivor(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _events.ScheduleEvent(EVENT_ORC_SURVIVOR_TALK, 5s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        // ArkCORE scheduled one event and handled another, so its survivors never talked (own fix)
        if (_events.ExecuteEvent() == EVENT_ORC_SURVIVOR_TALK)
        {
            me->HandleEmoteCommand(EMOTE_ONESHOT_TALK);
            _events.ScheduleEvent(EVENT_ORC_SURVIVOR_TALK, 5s, 10s);
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
};

/*######
## 36513 Trade Prince Gallywix (the camp): threatens the player and leaves in his own rocket
######*/

enum GallywixCamp
{
    EVENT_GALLYWIX_CAMP_EMOTE       = 1,
    EVENT_GALLYWIX_CAMP_TALK_1,
    EVENT_GALLYWIX_CAMP_TALK_2,
    EVENT_GALLYWIX_CAMP_TALK_3,
    EVENT_GALLYWIX_CAMP_LEAVE,
    EVENT_GALLYWIX_CAMP_DESPAWN,

    SAY_GALLYWIX_CAMP_BIGSHOT       = 0,
    SAY_GALLYWIX_CAMP_CRUSH         = 1,
    SAY_GALLYWIX_CAMP_MARK_MY_WORDS = 2,

    SPELL_FORCE_CAST_GALLYWIX_ROCKET = 68819, // the player summons 36514, the Trade Prince's sling rocket
    POINT_GALLYWIX_ROCKET           = 1,
};

struct npc_lost_isles_gallywix_camp : public ScriptedAI
{
    npc_lost_isles_gallywix_camp(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _talking = false;
        _events.ScheduleEvent(EVENT_GALLYWIX_CAMP_DESPAWN, 2min);
        _events.ScheduleEvent(EVENT_GALLYWIX_CAMP_EMOTE, 1s);
    }

    void IsSummonedBy(WorldObject* summoner) override
    {
        Player* player = summoner ? summoner->ToPlayer() : nullptr;
        if (!player)
            return;

        _playerGUID = player->GetGUID();
        me->SetFacingToObject(player);
        if (player->GetQuestStatus(QUEST_UP_UP_AND_AWAY) == QUEST_STATUS_INCOMPLETE
            && me->GetExactDist2d(player) < 15.0f)
        {
            _talking = true;
            _events.ScheduleEvent(EVENT_GALLYWIX_CAMP_TALK_1, 3s);
            _events.RescheduleEvent(EVENT_GALLYWIX_CAMP_DESPAWN, 2min);
        }
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE || id != POINT_GALLYWIX_ROCKET)
            return;

        if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
            DoCast(player, SPELL_FORCE_CAST_GALLYWIX_ROCKET, true);
        me->DespawnOrUnsummon(100ms);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID);
            switch (eventId)
            {
                case EVENT_GALLYWIX_CAMP_DESPAWN:
                    me->DespawnOrUnsummon();
                    break;
                case EVENT_GALLYWIX_CAMP_EMOTE:
                    if (!_talking)
                        me->HandleEmoteCommand(RAND(EMOTE_ONESHOT_EXCLAMATION, EMOTE_ONESHOT_RUDE, EMOTE_ONESHOT_ROAR));
                    _events.ScheduleEvent(EVENT_GALLYWIX_CAMP_EMOTE, 4s, 8s);
                    break;
                case EVENT_GALLYWIX_CAMP_TALK_1:
                    if (player)
                    {
                        me->SetFacingToObject(player);
                        Talk(SAY_GALLYWIX_CAMP_BIGSHOT, player);
                    }
                    _events.ScheduleEvent(EVENT_GALLYWIX_CAMP_TALK_2, 10s);
                    break;
                case EVENT_GALLYWIX_CAMP_TALK_2:
                    if (player)
                        Talk(SAY_GALLYWIX_CAMP_CRUSH, player);
                    _events.ScheduleEvent(EVENT_GALLYWIX_CAMP_TALK_3, 12s);
                    break;
                case EVENT_GALLYWIX_CAMP_TALK_3:
                    if (player)
                        Talk(SAY_GALLYWIX_CAMP_MARK_MY_WORDS, player);
                    _events.ScheduleEvent(EVENT_GALLYWIX_CAMP_LEAVE, 5s);
                    break;
                case EVENT_GALLYWIX_CAMP_LEAVE:
                    me->SetWalk(true);
                    me->GetMotionMaster()->MovePoint(POINT_GALLYWIX_ROCKET, 872.27f, 2743.14f, 122.82f);
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
    bool _talking = false;
};

/*######
## 36514 Sling Rocket (the Trade Prince's): blasts off over the sea
######*/

enum SlingRocket
{
    EVENT_SLING_ROCKET_START        = 1,

    SPELL_ROCKET_BLAST              = 66110,
    SPELL_FORCE_CAST_FROM_SLING_ROCKET = 68813, // the player casts 66127, kill credit and explosion
    POINT_SLING_ROCKET_END          = 1,
};

struct npc_lost_isles_sling_rocket_gallywix : public ScriptedAI
{
    npc_lost_isles_sling_rocket_gallywix(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        if (!summoner || !summoner->IsPlayer())
            return;

        DoCastSelf(SPELL_ROCKET_BLAST);
        me->SetCanFly(true);
        me->SetDisableGravity(true);
        me->SetSpeed(MOVE_RUN, 3.0f);
        me->SetSpeed(MOVE_FLIGHT, 3.0f);
        // ArkCORE's two-point path is not available: up and away to the west, over the sea
        Position destination = me->GetPosition();
        destination.RelocateOffset({ -60.0f, 0.0f, 80.0f, 0.0f });
        me->GetMotionMaster()->MovePoint(POINT_SLING_ROCKET_END, destination, FORCED_MOVEMENT_NONE, 0.0f, false, true);
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type == POINT_MOTION_TYPE && id == POINT_SLING_ROCKET_END)
            me->DespawnOrUnsummon(2s);
    }
};

/*######
## 196439 Rocket Sling: Up, Up & Away! (14244) launches the player
######*/

enum RocketSling
{
    SPELL_FORCE_CAST_FROM_ROCKET_SLING = 68806, // the player summons and rides the sling rocket 36505
};

struct go_lost_isles_rocket_sling : public GameObjectAI
{
    go_lost_isles_rocket_sling(GameObject* go) : GameObjectAI(go) { }

    bool GossipSelect(Player* player, uint32 /*sender*/, uint32 /*action*/) override
    {
        if (player->GetQuestStatus(QUEST_UP_UP_AND_AWAY) != QUEST_STATUS_INCOMPLETE)
            return false;

        CloseGossipMenuFor(player);
        player->CastSpell(player, SPELL_FORCE_CAST_FROM_ROCKET_SLING, true);
        return true;
    }
};

/*######
## 36505 Sling Rocket (the player's): flies the player to Foreman Dampwick and crashes
######*/

Position const SlingRocketDestination = { 943.8f, 2372.6f, 25.0f, 0.0f }; // over Foreman Dampwick (36470)

struct npc_lost_isles_sling_rocket : public VehicleAI
{
    npc_lost_isles_sling_rocket(Creature* creature) : VehicleAI(creature) { }

    void Reset() override
    {
        VehicleAI::Reset();
        me->SetCanFly(true);
        me->SetDisableGravity(true);
        me->SetSpeed(MOVE_FLIGHT, 3.5f);
        me->SetSpeed(MOVE_RUN, 3.5f);
        me->SetSpeed(MOVE_WALK, 3.5f);
    }

    void IsSummonedBy(WorldObject* summoner) override
    {
        if (Player* player = summoner ? summoner->ToPlayer() : nullptr)
        {
            _playerGUID = player->GetGUID();
            DoCastSelf(SPELL_ROCKET_BLAST, true);
        }
    }

    void PassengerBoarded(Unit* passenger, int8 /*seatId*/, bool apply) override
    {
        Player* player = passenger->ToPlayer();
        if (!player)
            return;

        if (apply)
            _events.ScheduleEvent(EVENT_SLING_ROCKET_START, 1s);
        else
        {
            DoCast(player, SPELL_FORCE_CAST_FROM_SLING_ROCKET, true);
            me->DespawnOrUnsummon(100ms);
        }
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (IsRideEnd(type) && id == POINT_SLING_ROCKET_END)
            DoCastSelf(SPELL_EJECT_ALL_PASSENGERS, true);
    }

    void UpdateAI(uint32 diff) override
    {
        VehicleAI::UpdateAI(diff);
        _events.Update(diff);

        if (_events.ExecuteEvent() == EVENT_SLING_ROCKET_START)
            MoveRide(me, POINT_SLING_ROCKET_END, SlingRocketDestination, true);
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
};

/*######
## Stages 5 and 6 (phases 180, 181): the Town-In-A-Box, the naga and the Oomlot attack
######*/

/*######
## The town's people: stand still, defend the town against the Oomlot warriors of stage 6 and walk home when drawn
## away. 36471 Foreman Dampwick, 38122 Bamm Megabomb, 38381 Brett "Coins" McQuid, 38511 Sally "Salvager" Sandscrew,
## 38513 Evol Fingers, 38514 Fizz Lighter, 38516 Sister Goldskimmer, 38517 Slinky Sharpshiv, 38518 Warrior-Matic NX-01,
## 38746 Gobber; the quest givers below build on it.
######*/

enum TownDefence
{
    PHASE_TOWN_UNDER_ATTACK         = 181,
    POINT_TOWN_DEFENDER_HOME        = 2016,
    AREA_GALLYWIX_DOCKS             = 4909,
    SPELL_EVOL_SUMMON_IMP           = 44163,
    NPC_SALLY_SANDSCREW             = 38511,
    NPC_EVOL_FINGERS                = 38513,
};

QuestLine const TownQuestLines[] =
{
    { 36471, QUEST_HOBART_NEEDS_YOU,        false, 0 },
    { 36471, QUEST_THREE_LITTLE_PYGMIES,    false, 1 },
    { 38122, QUEST_TRADING_UP,              false, 1 },
    { 38381, QUEST_NAGA_HIDE,               false, 0 },
    { 38124, QUEST_A_GOBLIN_IN_SHARKS_CLOTHING, false, 3 },
    { 38124, QUEST_ROCKIN_POWDER,           false, 4 },
    { 38124, QUEST_SHREDDER_SHUTDOWN,       false, 6 },
    { 38124, QUEST_RELEASE_THE_VALVES,      false, 7 },
    { 38124, QUEST_THE_SLAVE_PITS,          false, 8 },
    { 38120, QUEST_CLUSTER_CLUCK,           false, 0 },
    { 38120, QUEST_THE_BIGGEST_EGG_EVER,    false, 2 },
    { 38120, QUEST_INVASION_IMMINENT,       false, 5 },
    { 38120, QUEST_ROCKET_BOOT_BOOST,       true,  6 },
    { 38120, QUEST_VOLCANOTH,               false, 7 },
    { 38120, QUEST_ESCAPE_VELOCITY,         false, 8 },
    { 38738, QUEST_ZOMBIES_VS_ROCKET_BOOTS, false, 0 },
    { 38738, QUEST_ROCKET_BOOT_BOOST,       false, 1 },
    { 38738, QUEST_THE_ULTIMATE_FOOTBOMB_UNIFORM, false, 2 },
    { 38387, QUEST_TOWN_IN_A_BOX_UNDER_ATTACK, false, 0 },
    { 38387, QUEST_OOMLOT_VILLAGE,          false, 1 },
    { 38387, QUEST_UP_THE_VOLCANO,          false, 2 },
    { 38387, QUEST_MINE_DISPOSAL,           false, 3 },
    { 38387, QUEST_FINAL_CONFRONTATION,     false, 7 },
    { 38647, QUEST_FREE_THE_CAPTIVES,       false, 0 },
    { 38647, QUEST_OOMLOT_DEALT_WITH,       false, 1 },
    { 38432, QUEST_SURRENDER_OR_ELSE,       false, 0 },
    { 38432, QUEST_IRRESISTIBLE_POOL_PONY,  false, 1 },
    { 38432, QUEST_GET_BACK_TO_TOWN,        false, 2 },
};

struct npc_lost_isles_town_defender : public ScriptedAI
{
    npc_lost_isles_town_defender(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        // Sally fights on the docks in the last stage
        if (me->GetEntry() == NPC_SALLY_SANDSCREW && me->GetAreaId() == AREA_GALLYWIX_DOCKS)
            return;

        me->SetCombatMovement(false);
        me->SetReactState(REACT_DEFENSIVE);
        if (me->GetEntry() == NPC_EVOL_FINGERS)
            DoCastSelf(SPELL_EVOL_SUMMON_IMP);
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (!IsAtHome())
            return;

        if (who->GetEntry() == NPC_OOMLOT_WARRIOR && who->HasPhase(PHASE_TOWN_UNDER_ATTACK)
            && me->IsWithinDistInMap(who, 10.0f) && me->IsValidAttackTarget(who))
            AttackStart(who);
    }

    void sGossipSelect(Player* player, uint32 sender, uint32 action) override
    {
        HandleLostIslesGossipSpell(player, sender, action);
    }

    void sQuestAccept(Player* player, Quest const* quest) override
    {
        SayTownQuestLine(player, quest->GetQuestId(), false);
    }

    void sQuestReward(Player* player, Quest const* quest, uint32 /*opt*/) override
    {
        SayTownQuestLine(player, quest->GetQuestId(), true);
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!UpdateVictim())
            return;

        if (!IsAtHome())
            return;

        DoMeleeAttackIfReady();
    }

protected:
    void SayTownQuestLine(Player* player, uint32 questId, bool onReward)
    {
        for (QuestLine const& line : TownQuestLines)
            if (line.entry == me->GetEntry() && line.questId == questId && line.onReward == onReward)
                Talk(line.group, player);
    }

    // ArkCORE's CheckHomePosition: more than 10 yards from home, the defender stops and walks back
    bool IsAtHome()
    {
        if (me->GetDistance(me->GetHomePosition()) <= 10.0f)
            return true;

        me->AttackStop();
        me->GetMotionMaster()->MovePoint(POINT_TOWN_DEFENDER_HOME, me->GetHomePosition());
        return false;
    }
};

/*######
## 38738 Coach Crosscheck, 38387 Sassy Hardwrench (the town), 38647 Izzy: quest lines; Sassy stays out of the fight
######*/

enum SassyTown
{
    NPC_SASSY_HARDWRENCH_TOWN       = 38387,
    NPC_IZZY_TOWN                   = 38647,
    SPELL_PYGMY_HELMET              = 66987,
};

struct npc_lost_isles_town_quest_giver : public npc_lost_isles_town_defender
{
    npc_lost_isles_town_quest_giver(Creature* creature) : npc_lost_isles_town_defender(creature) { }

    void Reset() override
    {
        if (me->GetEntry() == NPC_IZZY_TOWN && me->GetAreaId() == AREA_GALLYWIX_DOCKS)
        {
            // On the docks Izzy fights alongside the player
            me->SetImmuneToAll(false);
            me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
            me->SetReactState(REACT_AGGRESSIVE);
            return;
        }

        npc_lost_isles_town_defender::Reset();
        if (me->GetEntry() == NPC_SASSY_HARDWRENCH_TOWN)
            me->SetReactState(REACT_PASSIVE);
    }

    void sQuestReward(Player* player, Quest const* quest, uint32 opt) override
    {
        if (me->GetEntry() == NPC_IZZY_TOWN && quest->GetQuestId() == QUEST_OOMLOT_VILLAGE)
            player->RemoveAurasDueToSpell(SPELL_PYGMY_HELMET);
        npc_lost_isles_town_defender::sQuestReward(player, quest, opt);
    }
};

/*######
## 38120 Hobart Grapplehammer: his quests, and The Biggest Egg Ever (24744) going into the Poultryizer
######*/

enum HobartGrapplehammer
{
    EVENT_HOBART_EGG_1              = 1,
    EVENT_HOBART_EGG_2,
    EVENT_HOBART_EGG_3,
    EVENT_HOBART_EGG_4,
    EVENT_HOBART_FOUNTAIN,

    SAY_HOBART_LOAD_THE_EGG         = 3,
    SAY_HOBART_SHUT_IT_DOWN         = 4,
    SAY_GREELY_GENIUS               = 0,
    SAY_GREELY_REVERSE_POLARITY     = 1,
    SAY_GREELY_THERE_IT_GOES        = 2,
    SAY_BAMM_COME_OVER_HERE         = 0,

    SPELL_EGG_FOUNTAIN              = 71608,
};

// ArkCORE's egg fountain: how many eggs fly each second, the Trade Prince's speech after the fourth second
std::array<uint8, 7> const EggFountainBursts = { 5, 3, 3, 3, 2, 2, 1 };

struct npc_lost_isles_hobart_grapplehammer : public npc_lost_isles_town_defender
{
    npc_lost_isles_hobart_grapplehammer(Creature* creature) : npc_lost_isles_town_defender(creature) { }

    void sQuestReward(Player* player, Quest const* quest, uint32 opt) override
    {
        switch (quest->GetQuestId())
        {
            case QUEST_CLUSTER_CLUCK:
                if (Creature* bamm = me->FindNearestCreature(NPC_BAMM_MEGABOMB, 30.0f))
                    bamm->AI()->Talk(SAY_BAMM_COME_OVER_HERE, player);
                break;
            case QUEST_THE_BIGGEST_EGG_EVER:
                _playerGUID = player->GetGUID();
                _events.Reset();
                _events.ScheduleEvent(EVENT_HOBART_EGG_1, 2s);
                break;
            default:
                break;
        }
        npc_lost_isles_town_defender::sQuestReward(player, quest, opt);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID);
            Creature* greely = ObjectAccessor::GetCreature(*me, _greelyGUID);
            switch (eventId)
            {
                case EVENT_HOBART_EGG_1:
                    if (Creature* found = me->FindNearestCreature(NPC_ASSISTANT_GREELY, 30.0f))
                        _greelyGUID = found->GetGUID();
                    if (player)
                        Talk(SAY_HOBART_LOAD_THE_EGG, player);
                    _events.ScheduleEvent(EVENT_HOBART_EGG_2, 2s);
                    break;
                case EVENT_HOBART_EGG_2:
                    if (greely && player)
                        greely->AI()->Talk(SAY_GREELY_GENIUS, player);
                    _events.ScheduleEvent(EVENT_HOBART_EGG_3, 7s);
                    break;
                case EVENT_HOBART_EGG_3:
                    if (greely && player)
                        greely->AI()->Talk(SAY_GREELY_REVERSE_POLARITY, player);
                    _events.ScheduleEvent(EVENT_HOBART_EGG_4, 7s);
                    break;
                case EVENT_HOBART_EGG_4:
                    if (greely && player)
                        greely->AI()->Talk(SAY_GREELY_THERE_IT_GOES, player);
                    _fountainBurst = 0;
                    if (greely)
                        if (Creature* bunny = greely->FindNearestCreature(NPC_FIREWORKS_BUNNY, 15.0f))
                            _bunnyGUID = bunny->GetGUID();
                    _events.ScheduleEvent(EVENT_HOBART_FOUNTAIN, 3s);
                    break;
                case EVENT_HOBART_FOUNTAIN:
                    if (Creature* bunny = ObjectAccessor::GetCreature(*me, _bunnyGUID))
                        for (uint8 i = 0; i < EggFountainBursts[_fountainBurst]; ++i)
                            bunny->CastSpell(bunny, SPELL_EGG_FOUNTAIN, true);
                    if (_fountainBurst == 3 && player)
                        Talk(SAY_HOBART_SHUT_IT_DOWN, player);
                    if (++_fountainBurst < EggFountainBursts.size())
                        _events.ScheduleEvent(EVENT_HOBART_FOUNTAIN, 1s);
                    break;
                default:
                    break;
            }
        }

        npc_lost_isles_town_defender::UpdateAI(diff);
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
    ObjectGuid _greelyGUID;
    ObjectGuid _bunnyGUID;
    uint8 _fountainBurst = 0;
};

/*######
## 38432 Megs Dreadshredder: Surrender or Else! (24868) and Get Back to Town (24897)
######*/

enum MegsDreadshredder
{
    SPELL_SURRENDER_OR_ELSE_ACCEPT  = 72001,
};

struct npc_lost_isles_megs_dreadshredder : public npc_lost_isles_town_defender
{
    npc_lost_isles_megs_dreadshredder(Creature* creature) : npc_lost_isles_town_defender(creature) { }

    void sQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() == QUEST_SURRENDER_OR_ELSE)
            player->CastSpell(player, SPELL_SURRENDER_OR_ELSE_ACCEPT, true);
        else if (quest->GetQuestId() == QUEST_GET_BACK_TO_TOWN)
            player->CastSpell(player, SPELL_PYGMY_HELMET, true);
        npc_lost_isles_town_defender::sQuestAccept(player, quest);
    }
};

/*######
## 38111 Wild Clucker: Cluster Cluck (24671) fits it with fireworks
######*/

enum WildClucker
{
    EVENT_CLUCKER_CHECK_PLAYER      = 1,
    EVENT_CLUCKER_DESPAWN,
};

struct npc_lost_isles_wild_clucker : public ScriptedAI
{
    npc_lost_isles_wild_clucker(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        me->RemoveAllAuras();
        me->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
        _playerGUID.Clear();
        _finished = false;
    }

    // Clickable only for players on the quest
    void MoveInLineOfSight(Unit* who) override
    {
        if (me->HasNpcFlag(UNIT_NPC_FLAG_SPELLCLICK) || _finished)
            return;

        Player* player = who->ToPlayer();
        if (player && player->GetQuestStatus(QUEST_CLUSTER_CLUCK) == QUEST_STATUS_INCOMPLETE)
        {
            _playerGUID = player->GetGUID();
            me->SetNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
            _events.RescheduleEvent(EVENT_CLUCKER_CHECK_PLAYER, 10s);
        }
    }

    void SpellHit(Unit* caster, SpellInfo const* /*spell*/) override
    {
        Player* player = caster ? caster->ToPlayer() : nullptr;
        if (!player || _finished || player->GetQuestStatus(QUEST_CLUSTER_CLUCK) != QUEST_STATUS_INCOMPLETE)
            return;

        _finished = true;
        me->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
        player->KilledMonsterCredit(NPC_CLUSTER_CLUCK_CREDIT);
        _events.CancelEvent(EVENT_CLUCKER_CHECK_PLAYER);
        _events.ScheduleEvent(EVENT_CLUCKER_DESPAWN, 10s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_CLUCKER_CHECK_PLAYER:
                {
                    Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID);
                    if (!player || player->GetQuestStatus(QUEST_CLUSTER_CLUCK) != QUEST_STATUS_INCOMPLETE)
                    {
                        me->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
                        break;
                    }
                    _events.ScheduleEvent(EVENT_CLUCKER_CHECK_PLAYER, 10s);
                    break;
                }
                case EVENT_CLUCKER_DESPAWN:
                    me->DespawnOrUnsummon();
                    break;
                default:
                    break;
            }
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
    bool _finished = false;
};

/*######
## 38195 Wild Clucker Egg and 38187 Spiny Raptor: Trading Up (24741). The egg lures the nearest raptor into the raptor
## trap beside it; the trapped raptor leaves a spiny raptor egg.
######*/

enum TradingUp
{
    EVENT_EGG_FIND_RAPTOR           = 1,
    EVENT_EGG_HATCH,
    EVENT_EGG_DESPAWN,

    EVENT_RAPTOR_WALK               = 1,
    EVENT_RAPTOR_RESET,
    EVENT_RAPTOR_TRAPPED,
    EVENT_RAPTOR_DESPAWN,

    SPELL_SUMMON_SPINY_RAPTOR_EGG   = 66726,
    SPELL_RAPTOR_TRAP_STUN          = 35309,
    SPELL_RAPTOR_TRAPPED            = 66723,

    ACTION_RAPTOR_LURE              = 1,
    ACTION_EGG_HATCH                = 2,
    DATA_TRAP_GUID                  = 2,
    DATA_EGG_GUID                   = 3,
    POINT_RAPTOR_TRAP               = 1,
};

struct npc_lost_isles_wild_clucker_egg : public ScriptedAI
{
    npc_lost_isles_wild_clucker_egg(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _playerGUID = me->GetOwnerGUID();
        me->SetControlled(true, UNIT_STATE_ROOT);
        _events.ScheduleEvent(EVENT_EGG_FIND_RAPTOR, 500ms);
    }

    void DoAction(int32 action) override
    {
        if (action == ACTION_EGG_HATCH)
            _events.ScheduleEvent(EVENT_EGG_HATCH, 500ms);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_EGG_FIND_RAPTOR:
                {
                    Creature* raptor = me->FindNearestCreature(NPC_SPINY_RAPTOR, 30.0f);
                    if (!raptor)
                        if ((raptor = me->FindNearestCreature(NPC_SPINY_RAPTOR, 30.0f, false)))
                            raptor->Respawn();
                    GameObject* trap = me->FindNearestGameObject(GO_RAPTOR_TRAP, 3.0f);
                    if (!raptor || !trap || _playerGUID.IsEmpty())
                        break;

                    raptor->RemoveAllAuras();
                    raptor->SetControlled(false, UNIT_STATE_STUNNED);
                    raptor->AI()->SetGUID(_playerGUID, DATA_PLAYER_GUID);
                    raptor->AI()->SetGUID(trap->GetGUID(), DATA_TRAP_GUID);
                    raptor->AI()->SetGUID(me->GetGUID(), DATA_EGG_GUID);
                    raptor->AI()->DoAction(ACTION_RAPTOR_LURE);
                    break;
                }
                case EVENT_EGG_HATCH:
                    DoCastSelf(SPELL_SUMMON_SPINY_RAPTOR_EGG, true);
                    _events.ScheduleEvent(EVENT_EGG_DESPAWN, 100ms);
                    break;
                case EVENT_EGG_DESPAWN:
                    me->DespawnOrUnsummon();
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
};

struct npc_lost_isles_spiny_raptor : public ScriptedAI
{
    npc_lost_isles_spiny_raptor(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        me->SetControlled(false, UNIT_STATE_STUNNED);
        me->RemoveAurasDueToSpell(SPELL_FEIGN_DEATH);
        me->SetImmuneToAll(false);
        me->SetReactState(REACT_AGGRESSIVE);
    }

    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        switch (id)
        {
            case DATA_PLAYER_GUID:
                _playerGUID = guid;
                break;
            case DATA_TRAP_GUID:
                _trapGUID = guid;
                break;
            case DATA_EGG_GUID:
                _eggGUID = guid;
                break;
            default:
                break;
        }
    }

    void DoAction(int32 action) override
    {
        if (action != ACTION_RAPTOR_LURE || _playerGUID.IsEmpty() || _trapGUID.IsEmpty())
            return;

        _events.ScheduleEvent(EVENT_RAPTOR_WALK, 100ms);
        _events.ScheduleEvent(EVENT_RAPTOR_RESET, 15s);
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE || id != POINT_RAPTOR_TRAP)
            return;

        // The trap snaps shut (ArkCORE did this in the trap's own script)
        _events.CancelEvent(EVENT_RAPTOR_RESET);
        if (GameObject* trap = ObjectAccessor::GetGameObject(*me, _trapGUID))
            trap->UseDoorOrButton();
        DoCastSelf(SPELL_RAPTOR_TRAP_STUN, true);
        DoCastSelf(SPELL_FEIGN_DEATH, true);
        me->SetControlled(true, UNIT_STATE_STUNNED);
        DoCastSelf(SPELL_RAPTOR_TRAPPED, true);
    }

    void SpellHit(Unit* /*caster*/, SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_RAPTOR_TRAPPED)
            _events.ScheduleEvent(EVENT_RAPTOR_TRAPPED, 2500ms);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_RAPTOR_RESET:
                    Reset();
                    me->GetMotionMaster()->MoveTargetedHome();
                    break;
                case EVENT_RAPTOR_WALK:
                    me->SetReactState(REACT_PASSIVE);
                    me->SetImmuneToAll(true);
                    if (GameObject* trap = ObjectAccessor::GetGameObject(*me, _trapGUID))
                        me->GetMotionMaster()->MovePoint(POINT_RAPTOR_TRAP, trap->GetPosition());
                    break;
                case EVENT_RAPTOR_TRAPPED:
                    if (Creature* egg = ObjectAccessor::GetCreature(*me, _eggGUID))
                        egg->AI()->DoAction(ACTION_EGG_HATCH);
                    _events.ScheduleEvent(EVENT_RAPTOR_DESPAWN, 500ms);
                    break;
                case EVENT_RAPTOR_DESPAWN:
                    _events.Reset();
                    if (GameObject* trap = ObjectAccessor::GetGameObject(*me, _trapGUID))
                        trap->ResetDoorOrButton();
                    me->DespawnOrUnsummon(10ms);
                    break;
                default:
                    break;
            }
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
    ObjectGuid _trapGUID;
    ObjectGuid _eggGUID;
};

/*######
## 38224 Mechachicken: drops The Biggest Egg Ever (24744)
######*/

enum Mechachicken
{
    SPELL_SUMMON_BIGGEST_EGG_VISUAL = 71422,
};

struct npc_lost_isles_mechachicken : public ScriptedAI
{
    npc_lost_isles_mechachicken(Creature* creature) : ScriptedAI(creature) { }

    void JustDied(Unit* killer) override
    {
        Player* player = killer ? killer->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
        if (player && player->GetQuestStatus(QUEST_THE_BIGGEST_EGG_EVER) == QUEST_STATUS_INCOMPLETE)
            DoCastSelf(SPELL_SUMMON_BIGGEST_EGG_VISUAL, true);
    }
};

/*######
## 38227 The Biggest Egg Ever Spell Visual: places the egg
######*/

struct npc_lost_isles_biggest_egg_visual : public ScriptedAI
{
    npc_lost_isles_biggest_egg_visual(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        if (_eggPlaced)
            return;

        _eggPlaced = true;
        // A lifetime of two minutes so the summoned egg cannot outlive the quest scene (own; ArkCORE gave none)
        me->SummonGameObject(GO_THE_BIGGEST_EGG_EVER, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(),
            me->GetOrientation(), 0.0f, 0.0f, 0.0f, 0.0f, 2 * MINUTE);
    }

private:
    bool _eggPlaced = false;
};

/*######
## 38359 Vashj'elan Warrior: leaves the players with the naga hatchlings alone (Surrender or Else!), frenzies
######*/

enum VashjelanWarrior
{
    EVENT_WARRIOR_ENRAGE_COOLDOWN   = 1,
    SPELL_NAGA_ENRAGE               = 8599,
    SAY_WARRIOR_FRENZY              = 0,
};

// Players taking the naga leader's surrender carry its hatchlings: the naga do not attack them
bool IsUnderNagaTruce(Unit* who)
{
    Player* player = who->ToPlayer();
    if (!player)
        return false;

    QuestStatus status = player->GetQuestStatus(QUEST_SURRENDER_OR_ELSE);
    return status == QUEST_STATUS_INCOMPLETE || status == QUEST_STATUS_COMPLETE;
}

struct npc_lost_isles_vashjelan_warrior : public ScriptedAI
{
    npc_lost_isles_vashjelan_warrior(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _enrageCooldown = false;
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (IsUnderNagaTruce(who))
        {
            if (me->GetVictim() == who)
                me->AttackStop();
            return;
        }

        ScriptedAI::MoveInLineOfSight(who);
    }

    void DamageTaken(Unit* attacker, uint32& /*damage*/, DamageEffectType /*type*/, SpellSchoolMask /*school*/) override
    {
        if (_enrageCooldown || me->GetHealthPct() >= 40.0f)
            return;

        _enrageCooldown = true;
        _events.ScheduleEvent(EVENT_WARRIOR_ENRAGE_COOLDOWN, 2min);
        Talk(SAY_WARRIOR_FRENZY, attacker);
        DoCastSelf(SPELL_NAGA_ENRAGE, true);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (_events.ExecuteEvent() == EVENT_WARRIOR_ENRAGE_COOLDOWN)
            _enrageCooldown = false;

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
    bool _enrageCooldown = false;
};

/*######
## 38360 Vashj'elan Siren: guards three to six naga hatchlings underwater; when she dies they can be picked up
## (Irresistible Pool Pony, 24864)
######*/

enum VashjelanSiren
{
    EVENT_SIREN_NOVA_COOLDOWN       = 1,
    EVENT_SIREN_BOLT_COOLDOWN,

    SPELL_SIREN_FROST_NOVA          = 11831,
    SPELL_SIREN_FROSTBOLT           = 9672,
    AREA_NAGA_WATERS                = 4877,
    ACTION_HATCHLING_MASTER_KILLED  = 1,
};

struct npc_lost_isles_vashjelan_siren : public ScriptedAI
{
    npc_lost_isles_vashjelan_siren(Creature* creature) : ScriptedAI(creature), _hatchlings(creature) { }

    void Reset() override
    {
        _events.Reset();
        _novaCooldown = false;
        _boltCooldown = false;
        _hatchlings.DespawnAll();

        if (me->GetAreaId() != AREA_NAGA_WATERS || me->GetPositionZ() > -5.0f)
            return;

        // TDB 434 has spell clicks for 44579 only of ArkCORE's three hatchling entries
        for (uint32 i = urand(3, 6); i > 0; --i)
            me->SummonCreature(NPC_NAGA_HATCHLING_CLICKABLE, me->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN);
    }

    void JustSummoned(Creature* summon) override
    {
        _hatchlings.Summon(summon);
    }

    void SummonedCreatureDespawn(Creature* summon) override
    {
        _hatchlings.Despawn(summon);
    }

    void SummonedCreatureDies(Creature* summon, Unit* /*killer*/) override
    {
        _hatchlings.Despawn(summon);
    }

    void JustEngagedWith(Unit* who) override
    {
        if (!_novaCooldown && roll_chance_i(30))
        {
            DoCast(who, SPELL_SIREN_FROST_NOVA);
            _novaCooldown = true;
            _events.ScheduleEvent(EVENT_SIREN_NOVA_COOLDOWN, 8s);
        }
    }

    void DamageTaken(Unit* attacker, uint32& /*damage*/, DamageEffectType /*type*/, SpellSchoolMask /*school*/) override
    {
        if (!attacker)
            return;

        if (!_novaCooldown && roll_chance_i(30))
        {
            DoCast(attacker, SPELL_SIREN_FROST_NOVA);
            _novaCooldown = true;
            _events.ScheduleEvent(EVENT_SIREN_NOVA_COOLDOWN, 8s);
            return;
        }
        if (!_boltCooldown && roll_chance_i(50))
        {
            DoCast(attacker, SPELL_SIREN_FROSTBOLT);
            _boltCooldown = true;
            _events.ScheduleEvent(EVENT_SIREN_BOLT_COOLDOWN, 10s);
        }
    }

    void JustDied(Unit* killer) override
    {
        ObjectGuid playerGUID;
        if (Player* player = killer ? killer->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr)
            if (player->GetQuestStatus(QUEST_IRRESISTIBLE_POOL_PONY) == QUEST_STATUS_INCOMPLETE)
                playerGUID = player->GetGUID();

        for (ObjectGuid const& guid : _hatchlings)
            if (Creature* hatchling = ObjectAccessor::GetCreature(*me, guid))
            {
                hatchling->AI()->SetGUID(playerGUID, DATA_PLAYER_GUID);
                hatchling->AI()->DoAction(ACTION_HATCHLING_MASTER_KILLED);
            }
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (IsUnderNagaTruce(who))
        {
            if (me->GetVictim() == who)
                me->AttackStop();
            return;
        }

        ScriptedAI::MoveInLineOfSight(who);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_SIREN_NOVA_COOLDOWN:
                    _novaCooldown = false;
                    break;
                case EVENT_SIREN_BOLT_COOLDOWN:
                    _boltCooldown = false;
                    break;
                default:
                    break;
            }
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
    SummonList _hatchlings;
    bool _novaCooldown = false;
    bool _boltCooldown = false;
};

/*######
## 44579 Naga Hatchling: follows its siren; once she is dead (or for TDB's lone hatchlings at once) a player on
## Irresistible Pool Pony picks it up, which summons a hatchling that follows the player
######*/

enum NagaHatchling
{
    EVENT_HATCHLING_FOLLOW          = 1,
    EVENT_HATCHLING_CHECK_MASTER,
    EVENT_HATCHLING_CHECK_PLAYER,

    FACTION_FRIENDLY                = 35,
    SOUND_HATCHLING_PICKED_UP       = 3437,
    SPELL_SUMMON_HATCHLING_01       = 71918,
    SPELL_SUMMON_HATCHLING_02       = 71919,
    SPELL_SUMMON_HATCHLING_03       = 83115,
    SPELL_SUMMON_HATCHLING_04       = 83116,
    SPELL_PROXIMITY_CONTROL_MASTER  = 71917,
    POINT_HATCHLING_SPREAD          = 1024,
};

struct npc_lost_isles_naga_hatchling : public ScriptedAI
{
    npc_lost_isles_naga_hatchling(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        if (!summoner)
            return;

        _masterGUID = summoner->GetGUID();
        me->GetMotionMaster()->MovePoint(POINT_HATCHLING_SPREAD, me->GetNearPosition(frand(3.0f, 5.0f),
            frand(0.0f, 2 * float(M_PI))));
        _events.ScheduleEvent(EVENT_HATCHLING_CHECK_MASTER, 2s);
    }

    void OnSpellClick(Unit* clicker, bool& /*result*/) override
    {
        Player* player = clicker->ToPlayer();
        if (!player || player->GetQuestStatus(QUEST_IRRESISTIBLE_POOL_PONY) != QUEST_STATUS_INCOMPLETE)
            return;

        me->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
        me->SetFaction(FACTION_FRIENDLY);
        me->PlayDistanceSound(SOUND_HATCHLING_PICKED_UP);
        player->CastSpell(me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(),
            RAND(SPELL_SUMMON_HATCHLING_01, SPELL_SUMMON_HATCHLING_02, SPELL_SUMMON_HATCHLING_03,
                SPELL_SUMMON_HATCHLING_04), true);
        me->DespawnOrUnsummon(10ms);
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type == POINT_MOTION_TYPE && id == POINT_HATCHLING_SPREAD)
            _events.ScheduleEvent(EVENT_HATCHLING_FOLLOW, 10ms);
    }

    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        if (id == DATA_PLAYER_GUID)
            _playerGUID = guid;
    }

    void DoAction(int32 action) override
    {
        if (action != ACTION_HATCHLING_MASTER_KILLED)
            return;

        _masterGUID.Clear();
        _events.Reset();
        me->SetNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
        me->SetFaction(FACTION_FRIENDLY);
        if (_playerGUID.IsEmpty())
            me->DespawnOrUnsummon(10ms);
        else
            _events.ScheduleEvent(EVENT_HATCHLING_CHECK_PLAYER, 1s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_HATCHLING_CHECK_MASTER:
                    if (!ObjectAccessor::GetCreature(*me, _masterGUID))
                        me->DespawnOrUnsummon(10ms);
                    else
                        _events.ScheduleEvent(EVENT_HATCHLING_CHECK_MASTER, 2s);
                    break;
                case EVENT_HATCHLING_CHECK_PLAYER:
                {
                    Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID);
                    if (!player || player->GetQuestStatus(QUEST_IRRESISTIBLE_POOL_PONY) != QUEST_STATUS_INCOMPLETE)
                        me->DespawnOrUnsummon(10ms);
                    else
                        _events.ScheduleEvent(EVENT_HATCHLING_CHECK_PLAYER, 1s);
                    break;
                }
                case EVENT_HATCHLING_FOLLOW:
                    if (Creature* siren = ObjectAccessor::GetCreature(*me, _masterGUID))
                        me->GetMotionMaster()->MoveFollow(siren, frand(3.0f, 4.5f), frand(1.65f, 4.63f));
                    break;
                default:
                    break;
            }
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
    ObjectGuid _masterGUID;
};

/*######
## 44588, 44589, 44590, 44591 Naga Hatchling: a picked-up hatchling following the player (24864)
######*/

struct npc_lost_isles_naga_hatchling_follower : public ScriptedAI
{
    npc_lost_isles_naga_hatchling_follower(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _events.ScheduleEvent(EVENT_HATCHLING_CHECK_PLAYER, 3s);
    }

    void IsSummonedBy(WorldObject* summoner) override
    {
        DoCastSelf(SPELL_PROXIMITY_CONTROL_MASTER, true);
        if (Player* player = summoner ? summoner->ToPlayer() : nullptr)
        {
            _playerGUID = player->GetGUID();
            me->SetFaction(FACTION_FRIENDLY);
            me->SetImmuneToAll(true);
            player->KilledMonsterCredit(NPC_NAGA_HATCHLING_CREDIT);
        }
        me->GetMotionMaster()->MovePoint(POINT_HATCHLING_SPREAD, me->GetNearPosition(frand(2.0f, 4.0f),
            frand(0.0f, 2 * float(M_PI))));
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type == POINT_MOTION_TYPE && id == POINT_HATCHLING_SPREAD)
            _events.ScheduleEvent(EVENT_HATCHLING_FOLLOW, 10ms);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID);
            switch (eventId)
            {
                case EVENT_HATCHLING_CHECK_PLAYER:
                {
                    // Gone with the quest turned in (ArkCORE) or dropped (own)
                    QuestStatus status = player ? player->GetQuestStatus(QUEST_IRRESISTIBLE_POOL_PONY)
                        : QUEST_STATUS_NONE;
                    if (status == QUEST_STATUS_REWARDED || status == QUEST_STATUS_NONE)
                    {
                        me->DespawnOrUnsummon();
                        break;
                    }
                    _events.ScheduleEvent(EVENT_HATCHLING_CHECK_PLAYER, 1s);
                    break;
                }
                case EVENT_HATCHLING_FOLLOW:
                    if (player)
                        me->GetMotionMaster()->MoveFollow(player, frand(3.0f, 4.5f), frand(1.65f, 4.63f));
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
};

/*######
## 38457 Naga Hatchling: the hatchlings held hostage for Surrender or Else! (24868)
######*/

struct npc_lost_isles_naga_hatchling_hostage : public ScriptedAI
{
    npc_lost_isles_naga_hatchling_hostage(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _events.ScheduleEvent(EVENT_HATCHLING_CHECK_PLAYER, 3s);
    }

    void IsSummonedBy(WorldObject* summoner) override
    {
        if (Player* player = summoner ? summoner->ToPlayer() : nullptr)
            _playerGUID = player->GetGUID();
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (_events.ExecuteEvent() == EVENT_HATCHLING_CHECK_PLAYER)
        {
            Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID);
            if (!player || player->GetQuestStatus(QUEST_SURRENDER_OR_ELSE) != QUEST_STATUS_INCOMPLETE)
                me->DespawnOrUnsummon();
            else
                _events.ScheduleEvent(EVENT_HATCHLING_CHECK_PLAYER, 1s);
        }
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
};

/*######
## 38455 Ace and 38448 Faceless of the Deep: Surrender or Else! (24868). Ace leads the player with the hatchlings to
## the naga's master, who refuses, sends Ace running and fights.
######*/

enum SurrenderOrElse
{
    EVENT_ACE_DESPAWN               = 1,
    EVENT_ACE_TALK_1,
    EVENT_ACE_TALK_2,
    EVENT_ACE_WAIT_FOR_FACELESS,
    EVENT_ACE_RUN_AWAY,
    EVENT_ACE_LEAVE,

    EVENT_FACELESS_DESPAWN          = 1,
    EVENT_FACELESS_NEAR_COOLDOWN,
    EVENT_FACELESS_CHECK_PLAYERS,
    EVENT_FACELESS_BARRICADE,
    EVENT_FACELESS_TALK_1,
    EVENT_FACELESS_TALK_2,
    EVENT_FACELESS_TALK_3,
    EVENT_FACELESS_TALK_4,
    EVENT_FACELESS_JUMP,
    EVENT_FACELESS_ATTACK,

    SPELL_GOBLIN_BANNER_AURA        = 72077,
    SPELL_FACELESS_BEAM             = 72076,
    SPELL_SURRENDER_KILL_CREDIT     = 72035,

    ACTION_FACELESS_START_TALK      = 1,
    ACTION_ACE_RUN_AWAY             = 2,
    DATA_ACE_GUID                   = 4,

    SAY_FACELESS_WHO_DARES          = 0,
    SAY_FACELESS_LITTLE_GOBLINS     = 1,
    SAY_FACELESS_CARE_NOTHING       = 2,
    SAY_FACELESS_YOU_WILL_DIE       = 3,
};

constexpr float FacelessTalkDistance = 47.0f;
constexpr float FacelessHoverHeight = 9.0f;

struct npc_lost_isles_ace : public ScriptedAI
{
    npc_lost_isles_ace(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        DoCastSelf(SPELL_GOBLIN_BANNER_AURA, true);
        _events.ScheduleEvent(EVENT_ACE_DESPAWN, 250s);
    }

    void IsSummonedBy(WorldObject* summoner) override
    {
        Player* player = summoner ? summoner->ToPlayer() : nullptr;
        if (!player)
            return;

        // ArkCORE walked Ace along its own paths; he follows the player to the naga's master instead
        _playerGUID = player->GetGUID();
        me->GetMotionMaster()->MoveFollow(player, 3.0f, float(M_PI_2));
        _events.ScheduleEvent(EVENT_ACE_TALK_1, 1s);
    }

    void DoAction(int32 action) override
    {
        if (action == ACTION_ACE_RUN_AWAY && !_playerGUID.IsEmpty())
            _events.ScheduleEvent(EVENT_ACE_RUN_AWAY, 1s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID);
            switch (eventId)
            {
                case EVENT_ACE_DESPAWN:
                    me->DespawnOrUnsummon();
                    break;
                case EVENT_ACE_TALK_1:
                    if (player)
                        me->Say(BCT_ACE_SHORT_LEASH, player);
                    _events.ScheduleEvent(EVENT_ACE_TALK_2, 5s);
                    break;
                case EVENT_ACE_TALK_2:
                    if (player)
                        me->Say(BCT_ACE_READY, player);
                    _events.ScheduleEvent(EVENT_ACE_WAIT_FOR_FACELESS, 1s);
                    break;
                case EVENT_ACE_WAIT_FOR_FACELESS:
                    // Waits until the player and the Faceless are close together
                    if (Creature* faceless = me->FindNearestCreature(NPC_FACELESS_OF_THE_DEEP, 50.0f))
                        if (player && player->GetExactDist2d(faceless) < FacelessTalkDistance)
                        {
                            faceless->AI()->SetGUID(me->GetGUID(), DATA_ACE_GUID);
                            faceless->AI()->DoAction(ACTION_FACELESS_START_TALK);
                            break;
                        }
                    _events.ScheduleEvent(EVENT_ACE_WAIT_FOR_FACELESS, 1s);
                    break;
                case EVENT_ACE_RUN_AWAY:
                    if (player)
                        me->Say(BCT_ACE_OUT_OF_HERE, player);
                    _events.ScheduleEvent(EVENT_ACE_LEAVE, 2s);
                    break;
                case EVENT_ACE_LEAVE:
                {
                    me->GetMotionMaster()->Clear();
                    float angle = player ? player->GetAngle(me) : me->GetOrientation();
                    me->GetMotionMaster()->MovePoint(0, me->GetNearPosition(30.0f, angle - me->GetOrientation()));
                    me->DespawnOrUnsummon(8s);
                    break;
                }
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
};

struct npc_lost_isles_faceless_of_the_deep : public ScriptedAI
{
    npc_lost_isles_faceless_of_the_deep(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _playerGUID.Clear();
        _aceGUID.Clear();
        _playerNear = false;
        _fighting = false;
        me->SetReactState(REACT_PASSIVE);
        me->SetImmuneToPC(true);
        me->SetSpeed(MOVE_WALK, 0.15f);
        _events.ScheduleEvent(EVENT_FACELESS_CHECK_PLAYERS, 1s);
        _events.ScheduleEvent(EVENT_FACELESS_BARRICADE, 1s);
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoCastSelf(SPELL_SURRENDER_KILL_CREDIT, true);
    }

    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        if (id == DATA_ACE_GUID)
            _aceGUID = guid;
    }

    void DoAction(int32 action) override
    {
        if (action != ACTION_FACELESS_START_TALK || _playerGUID.IsEmpty())
            return;

        _events.ScheduleEvent(EVENT_FACELESS_TALK_1, 100ms);
        _events.RescheduleEvent(EVENT_FACELESS_DESPAWN, 75s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID);
            switch (eventId)
            {
                case EVENT_FACELESS_DESPAWN:
                    me->DespawnOrUnsummon(10ms);
                    break;
                case EVENT_FACELESS_NEAR_COOLDOWN:
                    _playerNear = false;
                    break;
                case EVENT_FACELESS_CHECK_PLAYERS:
                    CheckPlayerNear();
                    _events.ScheduleEvent(EVENT_FACELESS_CHECK_PLAYERS, 1s);
                    break;
                case EVENT_FACELESS_BARRICADE:
                    // ArkCORE cast the beams on every update; once a second is enough for the look
                    if (_playerNear && !_fighting)
                        for (uint8 i = 0; i < 2; ++i)
                        {
                            Position pos = me->GetNearPosition(frand(9.5f, 11.0f), frand(0.0f, 2 * float(M_PI)));
                            me->CastSpell(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(),
                                SPELL_FACELESS_BEAM, true);
                        }
                    _events.ScheduleEvent(EVENT_FACELESS_BARRICADE, 1s);
                    break;
                case EVENT_FACELESS_TALK_1:
                    if (player)
                        Talk(SAY_FACELESS_WHO_DARES, player);
                    _events.ScheduleEvent(EVENT_FACELESS_TALK_2, 6s);
                    break;
                case EVENT_FACELESS_TALK_2:
                    if (player)
                        Talk(SAY_FACELESS_LITTLE_GOBLINS, player);
                    _events.ScheduleEvent(EVENT_FACELESS_TALK_3, 6s);
                    break;
                case EVENT_FACELESS_TALK_3:
                    if (player)
                        Talk(SAY_FACELESS_CARE_NOTHING, player);
                    _events.ScheduleEvent(EVENT_FACELESS_TALK_4, 6s);
                    break;
                case EVENT_FACELESS_TALK_4:
                    if (player)
                    {
                        Talk(SAY_FACELESS_YOU_WILL_DIE, player);
                        if (Creature* ace = ObjectAccessor::GetCreature(*me, _aceGUID))
                            ace->AI()->DoAction(ACTION_ACE_RUN_AWAY);
                        std::list<Creature*> hostages;
                        player->GetAllMinionsByEntry(hostages, NPC_NAGA_HATCHLING_SURRENDER);
                        for (Creature* hostage : hostages)
                            hostage->DespawnOrUnsummon(100ms);
                    }
                    _events.ScheduleEvent(EVENT_FACELESS_JUMP, 4s);
                    break;
                case EVENT_FACELESS_JUMP:
                    if (player)
                    {
                        _fighting = true;
                        me->RemoveAllAuras();
                        me->SetHover(false);
                        me->SetFloatValue(UNIT_FIELD_HOVERHEIGHT, 0.0f);
                        me->SetSpeed(MOVE_WALK, 1.0f);
                        me->GetMotionMaster()->MoveJump(player->GetPosition(), 20.0f, 10.0f);
                    }
                    _events.ScheduleEvent(EVENT_FACELESS_ATTACK, 500ms);
                    break;
                case EVENT_FACELESS_ATTACK:
                    me->SetImmuneToPC(false);
                    me->SetReactState(REACT_AGGRESSIVE);
                    if (player)
                        AttackStart(player);
                    break;
                default:
                    break;
            }
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    // Rises over the water while a player on the quest is near
    void CheckPlayerNear()
    {
        if (_fighting)
            return;

        bool playerNear = false;
        if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
            if (player->GetQuestStatus(QUEST_SURRENDER_OR_ELSE) == QUEST_STATUS_INCOMPLETE
                && player->GetExactDist2d(me) < FacelessTalkDistance)
                playerNear = true;

        if (!playerNear)
            for (Player* player : GetPlayersInRange(me, FacelessTalkDistance * 0.85f))
                if (player->GetQuestStatus(QUEST_SURRENDER_OR_ELSE) == QUEST_STATUS_INCOMPLETE)
                {
                    _playerGUID = player->GetGUID();
                    playerNear = true;
                }

        if (playerNear != _playerNear)
            me->SetHover(playerNear);
        _playerNear = playerNear;

        if (playerNear)
        {
            me->SetFloatValue(UNIT_FIELD_HOVERHEIGHT, FacelessHoverHeight);
            _events.RescheduleEvent(EVENT_FACELESS_NEAR_COOLDOWN, 30s);
        }
        else
        {
            _playerGUID.Clear();
            me->SetFloatValue(UNIT_FIELD_HOVERHEIGHT, 0.0f);
        }
    }

    EventMap _events;
    ObjectGuid _playerGUID;
    ObjectGuid _aceGUID;
    bool _playerNear = false;
    bool _fighting = false;
};

/*######
## 38531 Oomlot Warrior: the attack on the town. The B.C. Eliminator kills it outright, anything else halves it, and
## it does no damage itself; in stage 7 the captured warriors stand still.
######*/

enum OomlotWarrior
{
    PHASE_UP_THE_VOLCANO            = 182,
};

struct npc_lost_isles_oomlot_warrior : public ScriptedAI
{
    npc_lost_isles_oomlot_warrior(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        me->SetReactState(REACT_PASSIVE);
        if (me->HasPhase(PHASE_UP_THE_VOLCANO))
        {
            me->SetCombatMovement(false);
            me->SetImmuneToAll(true);
        }
    }

    void DamageDealt(Unit* /*victim*/, uint32& damage, DamageEffectType /*type*/, SpellSchoolMask /*school*/) override
    {
        damage = 0;
    }

    void DamageTaken(Unit* attacker, uint32& damage, DamageEffectType /*type*/, SpellSchoolMask /*school*/) override
    {
        if (attacker && attacker->GetEntry() == NPC_BC_ELIMINATOR)
            damage = me->GetMaxHealth() + 1;
        else
            damage = me->GetMaxHealth() / 2 + 1;
    }
};

/*######
## 38526 B.C. Eliminator: the town's cannons in Town-In-A-Box: Under Attack (24901). A player clicks one to man it
## (72240) and fires with 72206. One nobody mans keeps firing at the attackers on its own while a player is near in
## stage 6: the NPC fire (72695, the client's "Fire Cannon - NPC") and a Cannon Blast (72207) on an Oomlot Warrior.
## No source has the NPC cannons' script; the spells are the client's.
######*/

enum BCEliminator
{
    SPELL_FIRE_CANNON_NPC           = 72695,
    SPELL_CANNON_BLAST              = 72207,
    EVENT_ELIMINATOR_FIRE           = 1,
};

constexpr float ELIMINATOR_RANGE = 70.0f;       // how far an unmanned cannon shoots
constexpr float ELIMINATOR_PLAYER_RANGE = 120.0f; // it only fires while a player is this near

struct npc_lost_isles_bc_eliminator : public VehicleAI
{
    npc_lost_isles_bc_eliminator(Creature* creature) : VehicleAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _events.ScheduleEvent(EVENT_ELIMINATOR_FIRE, randtime(2s, 6s));
    }

    void UpdateAI(uint32 diff) override
    {
        VehicleAI::UpdateAI(diff);
        _events.Update(diff);

        if (_events.ExecuteEvent() != EVENT_ELIMINATOR_FIRE)
            return;

        _events.ScheduleEvent(EVENT_ELIMINATOR_FIRE, randtime(4s, 8s));

        Vehicle* vehicle = me->GetVehicleKit();
        if (!me->HasPhase(PHASE_TOWN_UNDER_ATTACK) || (vehicle && vehicle->GetPassenger(0))
            || !me->SelectNearestPlayer(ELIMINATOR_PLAYER_RANGE))
            return;

        std::list<Creature*> warriors;
        me->GetCreatureListWithEntryInGrid(warriors, NPC_OOMLOT_WARRIOR, ELIMINATOR_RANGE);
        warriors.remove_if([](Creature* warrior) { return !warrior->IsAlive(); });
        if (warriors.empty())
            return;

        Creature* target = Acore::Containers::SelectRandomContainerElement(warriors);
        me->SetFacingToObject(target);
        DoCastSelf(SPELL_FIRE_CANNON_NPC, true);
        me->CastSpell(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ(), SPELL_CANNON_BLAST, true);
    }

private:
    EventMap _events;
};

/*######
## 38696 Yngwie: the Oomlot leader of Send a Message (24929)
######*/

enum Yngwie
{
    SAY_YNGWIE_AGGRO                = 0,
    SPELL_VOODOO_ILLUSION           = 75942,
};

struct npc_lost_isles_yngwie : public ScriptedAI
{
    npc_lost_isles_yngwie(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _illusionSummoned = false;
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        Talk(SAY_YNGWIE_AGGRO);
    }

    void DamageTaken(Unit* attacker, uint32& /*damage*/, DamageEffectType /*type*/, SpellSchoolMask /*school*/) override
    {
        if (_illusionSummoned || !attacker || !attacker->GetCharmerOrOwnerPlayerOrPlayerItself())
            return;

        if (me->GetHealthPct() < 30.0f)
        {
            _illusionSummoned = true;
            DoCastSelf(SPELL_VOODOO_ILLUSION, true);
        }
    }

private:
    bool _illusionSummoned = false;
};

/*######
## 38643 Goblin Captive and 38644 Oomlot Shaman: Free the Captives (24925)
######*/

enum FreeTheCaptives
{
    SPELL_SUMMON_OOMLOT_SHAMAN      = 72243,
    SPELL_ENVELOPING_WINDS_CAPTIVE  = 72522,
    SPELL_ENVELOPING_WINDS_SHAMAN   = 72518,

    ACTION_CAPTIVE_SHAMAN_ENGAGED   = 1,
    ACTION_CAPTIVE_SHAMAN_KILLED    = 2,
    ACTION_CAPTIVE_SHAMAN_GONE      = 3,

    SAY_CAPTIVE_FIRST               = 0,
    SAY_CAPTIVE_LAST                = 3,
};

struct npc_lost_isles_goblin_captive : public ScriptedAI
{
    npc_lost_isles_goblin_captive(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _playerGUID.Clear();
        DoCastSelf(SPELL_SUMMON_OOMLOT_SHAMAN, true);
        DoCastSelf(SPELL_ENVELOPING_WINDS_CAPTIVE, true);
    }

    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        if (id == DATA_PLAYER_GUID)
            _playerGUID = guid;
    }

    void DoAction(int32 action) override
    {
        switch (action)
        {
            case ACTION_CAPTIVE_SHAMAN_ENGAGED:
                me->RemoveAurasDueToSpell(SPELL_ENVELOPING_WINDS_CAPTIVE);
                me->RemoveAurasDueToSpell(SPELL_ENVELOPING_WINDS_SHAMAN);
                break;
            case ACTION_CAPTIVE_SHAMAN_KILLED:
                me->RemoveAurasDueToSpell(SPELL_SUMMON_OOMLOT_SHAMAN);
                if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
                    Talk(urand(SAY_CAPTIVE_FIRST, SAY_CAPTIVE_LAST), player);
                me->DespawnOrUnsummon(3s);
                break;
            case ACTION_CAPTIVE_SHAMAN_GONE:
                Reset();
                break;
            default:
                break;
        }
    }

private:
    ObjectGuid _playerGUID;
};

struct npc_lost_isles_oomlot_shaman : public ScriptedAI
{
    npc_lost_isles_oomlot_shaman(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        if (!summoner)
            return;

        _captiveGUID = summoner->GetGUID();
        me->SetFacingToObject(summoner);
        DoCastSelf(SPELL_ENVELOPING_WINDS_SHAMAN, true);
    }

    void JustEngagedWith(Unit* who) override
    {
        Player* player = who->GetCharmerOrOwnerPlayerOrPlayerItself();
        Creature* captive = ObjectAccessor::GetCreature(*me, _captiveGUID);
        if (!player || !captive)
            return;

        _playerGUID = player->GetGUID();
        me->RemoveAurasDueToSpell(SPELL_ENVELOPING_WINDS_SHAMAN);
        captive->AI()->SetGUID(player->GetGUID(), DATA_PLAYER_GUID);
        captive->AI()->DoAction(ACTION_CAPTIVE_SHAMAN_ENGAGED);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        if (Creature* captive = ObjectAccessor::GetCreature(*me, _captiveGUID))
            if (!_playerGUID.IsEmpty())
                captive->AI()->DoAction(ACTION_CAPTIVE_SHAMAN_GONE);
        me->DespawnOrUnsummon();
    }

    void JustDied(Unit* killer) override
    {
        Player* player = killer ? killer->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
        if (!player)
            return;

        if (Creature* captive = ObjectAccessor::GetCreature(*me, _captiveGUID))
        {
            captive->AI()->SetGUID(player->GetGUID(), DATA_PLAYER_GUID);
            captive->AI()->DoAction(ACTION_CAPTIVE_SHAMAN_KILLED);
        }
    }

private:
    ObjectGuid _captiveGUID;
    ObjectGuid _playerGUID;
};

/*######
## Stage 7 (phase 182): up the volcano
######*/

/*######
## Three Little Pygmies (24945): 38808 Gaahl, 38809 Malmo and 38810 Teloch each hold a goblin captive (38812, 50311,
## 50310). Attacking the pygmy turns its captive into a goblin zombie (38813, 38816, 38815) that fights for it.
######*/

enum ThreeLittlePygmies
{
    EVENT_CAPTIVE_CHECK             = 1,
    EVENT_ZOMBIE_ATTACK             = 1,
    EVENT_ZOMBIE_DESPAWN,

    SPELL_ZOMBIE_TRANSFORMATION     = 72935,
    SPELL_ZOMBIE_STUN_VISUAL        = 72866,
    SPELL_ZOMBIE_PLAGUE_STATE       = 72778,

    ACTION_CAPTIVE_ZOMBIFY          = 2,
    DATA_PYGMY_GUID                 = 5,
    DATA_CAPTIVE_GUID               = 6,
};

struct PygmyCaptive
{
    uint32 captive;
    uint32 pygmy;
    uint32 zombifySpell;
};

PygmyCaptive const PygmyCaptives[] =
{
    { NPC_GOBLIN_CAPTIVE_GAAHL,  NPC_GAAHL,  72936 }, // Summon Goblin Zombie - Oystein Nord (38813)
    { NPC_GOBLIN_CAPTIVE_MALMO,  NPC_MALMO,  72938 }, // Summon Goblin Zombie - Oystein Ost (38816)
    { NPC_GOBLIN_CAPTIVE_TELOCH, NPC_TELOCH, 72937 }, // Summon Goblin Zombie - Oystein Sor (38815)
};

PygmyCaptive const* GetPygmyCaptive(uint32 captiveEntry)
{
    for (PygmyCaptive const& captive : PygmyCaptives)
        if (captive.captive == captiveEntry)
            return &captive;
    return nullptr;
}

struct npc_lost_isles_pygmy_leader : public ScriptedAI
{
    npc_lost_isles_pygmy_leader(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _captiveGUID.Clear();
    }

    void JustEngagedWith(Unit* who) override
    {
        Player* player = who->GetCharmerOrOwnerPlayerOrPlayerItself();
        if (!player)
            return;

        if (Creature* captive = ObjectAccessor::GetCreature(*me, _captiveGUID))
        {
            captive->AI()->SetGUID(player->GetGUID(), DATA_PLAYER_GUID);
            captive->AI()->DoAction(ACTION_CAPTIVE_ZOMBIFY);
        }
    }

    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        if (id == DATA_CAPTIVE_GUID)
            _captiveGUID = guid;
    }

private:
    ObjectGuid _captiveGUID;
};

struct npc_lost_isles_pygmy_captive : public ScriptedAI
{
    npc_lost_isles_pygmy_captive(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _playerGUID.Clear();
        _pygmyGUID.Clear();
        _events.ScheduleEvent(EVENT_CAPTIVE_CHECK, 1s);
    }

    void SpellHit(Unit* caster, SpellInfo const* spell) override
    {
        PygmyCaptive const* data = GetPygmyCaptive(me->GetEntry());
        if (data && caster && caster->GetEntry() == data->pygmy && spell->Id == SPELL_ZOMBIE_TRANSFORMATION)
            caster->GetAI()->SetGUID(me->GetGUID(), DATA_CAPTIVE_GUID);
    }

    void JustSummoned(Creature* summon) override
    {
        if (Creature* pygmy = ObjectAccessor::GetCreature(*me, _pygmyGUID))
            summon->AI()->SetGUID(pygmy->GetGUID(), DATA_PYGMY_GUID);
        summon->AI()->SetGUID(_playerGUID, DATA_PLAYER_GUID);
        me->DespawnOrUnsummon(100ms);
    }

    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        if (id == DATA_PLAYER_GUID)
            _playerGUID = guid;
    }

    void DoAction(int32 action) override
    {
        if (action != ACTION_CAPTIVE_ZOMBIFY)
            return;

        if (PygmyCaptive const* data = GetPygmyCaptive(me->GetEntry()))
        {
            me->RemoveAurasDueToSpell(SPELL_ZOMBIE_TRANSFORMATION);
            DoCastSelf(data->zombifySpell, true);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (_events.ExecuteEvent() != EVENT_CAPTIVE_CHECK)
            return;

        PygmyCaptive const* data = GetPygmyCaptive(me->GetEntry());
        if (!data)
            return;

        if (_pygmyGUID.IsEmpty())
            if (Creature* pygmy = me->FindNearestCreature(data->pygmy, 10.0f))
                if (!pygmy->IsInCombat())
                    _pygmyGUID = pygmy->GetGUID();

        if (!me->HasAura(SPELL_ZOMBIE_TRANSFORMATION))
            if (Creature* pygmy = ObjectAccessor::GetCreature(*me, _pygmyGUID))
                pygmy->CastSpell(me, SPELL_ZOMBIE_TRANSFORMATION, true);

        _events.ScheduleEvent(EVENT_CAPTIVE_CHECK, 1s);
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
    ObjectGuid _pygmyGUID;
};

/*######
## 38753, 38813, 38815, 38816 Goblin Zombie: slow; the rocket boots of Zombies vs. Super Booster Rocket Boots (24942)
## count them. A pygmy's new zombie takes its side and goes for the player.
######*/

struct npc_lost_isles_goblin_zombie : public ScriptedAI
{
    npc_lost_isles_goblin_zombie(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        if (me->GetEntry() == NPC_GOBLIN_ZOMBIE)
            return;

        _events.Reset();
        me->SetSpeed(MOVE_WALK, 0.3f);
        me->SetSpeed(MOVE_RUN, 0.3f);
        DoCastSelf(SPELL_ZOMBIE_STUN_VISUAL, true);
        DoCastSelf(SPELL_ZOMBIE_PLAGUE_STATE, true);
        // In the cave the zombies only stand for the survivors to shoot (ArkCORE's path there is not available)
        if (me->GetAreaId() != AREA_ZOMBIE_CAVE)
        {
            _events.ScheduleEvent(EVENT_ZOMBIE_ATTACK, 1s);
            _events.ScheduleEvent(EVENT_ZOMBIE_DESPAWN, 30s);
        }
    }

    void SpellHit(Unit* caster, SpellInfo const* spell) override
    {
        if (spell->Id != SPELL_ZOMBIE_TARGETS_ROCKET_BOOTS)
            return;

        if (Player* player = caster ? caster->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr)
            player->KilledMonsterCredit(NPC_ZOMBIE_KILL_CREDIT);
    }

    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        if (id == DATA_PLAYER_GUID)
            _playerGUID = guid;
        else if (id == DATA_PYGMY_GUID)
            _pygmyGUID = guid;
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_ZOMBIE_ATTACK:
                    if (Creature* pygmy = ObjectAccessor::GetCreature(*me, _pygmyGUID))
                        me->SetFaction(pygmy->GetFaction());
                    if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
                        AttackStart(player);
                    break;
                case EVENT_ZOMBIE_DESPAWN:
                    if (me->IsInCombat())
                        _events.ScheduleEvent(EVENT_ZOMBIE_DESPAWN, 30s);
                    else
                        me->DespawnOrUnsummon(10ms);
                    break;
                default:
                    break;
            }
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
    ObjectGuid _pygmyGUID;
};

/*######
## 72886 Zombies vs. Super Booster Rocket Boots: Damage - Effect, 72944 Shoot: hit the goblin zombies only
######*/

class spell_lost_isles_zombie_targets : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_zombie_targets);

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove_if([](WorldObject* target) { return !IsZombie(target->GetEntry()); });
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_lost_isles_zombie_targets::FilterTargets,
            EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
    }
};

/*######
## 38409 Goblin Survivor, 38745 Kezan Citizen: shoot the zombies coming near; in the cave they only watch, on the
## docks they fight
######*/

enum TownShooter
{
    EVENT_SHOOTER_COOLDOWN          = 1,
};

struct npc_lost_isles_zombie_shooter : public ScriptedAI
{
    npc_lost_isles_zombie_shooter(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _cooldown = false;
        switch (me->GetAreaId())
        {
            case AREA_ZOMBIE_CAVE:
                me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                me->SetReactState(REACT_PASSIVE);
                break;
            case AREA_GALLYWIX_DOCKS:
                me->SetImmuneToAll(false);
                me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                me->SetReactState(REACT_AGGRESSIVE);
                break;
            default:
                break;
        }
    }

    void MoveInLineOfSight(Unit* who) override
    {
        // ArkCORE shot on every sighting; a short cooldown keeps it to one shot at a time (own)
        if (_cooldown || !IsZombie(who->GetEntry()) || who->GetDistance2d(me) >= 15.0f || !who->IsAlive())
            return;

        _cooldown = true;
        DoCast(who, SPELL_ZOMBIE_TARGETS_SHOOT, true);
        _events.ScheduleEvent(EVENT_SHOOTER_COOLDOWN, 2s, 4s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (_events.ExecuteEvent() == EVENT_SHOOTER_COOLDOWN)
            _cooldown = false;

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
    bool _cooldown = false;
};

/*######
## 38850 Volcanoth Champion: fights the goblins on the volcano, never far from its post
######*/

enum VolcanothChampion
{
    EVENT_CHAMPION_CHECK_FIGHT      = 1,
};

struct npc_lost_isles_volcanoth_champion : public ScriptedAI
{
    npc_lost_isles_volcanoth_champion(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _events.ScheduleEvent(EVENT_CHAMPION_CHECK_FIGHT, 1s, 2s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (_events.ExecuteEvent() == EVENT_CHAMPION_CHECK_FIGHT)
        {
            if (me->GetHomePosition().GetExactDist(me) > 15.0f)
            {
                me->CombatStop();
                me->GetMotionMaster()->MoveTargetedHome();
            }
            else if (!me->IsInCombat())
                if (Creature* target = FindGoblinTarget())
                {
                    AttackStart(target);
                    if (target->AI())
                        target->AI()->AttackStart(me);
                }
            _events.ScheduleEvent(EVENT_CHAMPION_CHECK_FIGHT, 9s, 10s);
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    // ArkCORE's FindNearestTarget: the nearest goblin within 10 yards that is not fighting yet
    Creature* FindGoblinTarget()
    {
        std::list<Creature*> goblins;
        me->GetCreatureListWithEntryInGrid(goblins,
            std::vector<uint32>{ NPC_GOBLIN_SURVIVOR_TOWN, NPC_KEZAN_CITIZEN_TOWN }, 10.0f);
        Creature* nearest = nullptr;
        for (Creature* goblin : goblins)
            if (!goblin->IsInCombat() && me->IsValidAttackTarget(goblin)
                && (!nearest || me->GetExactDist(goblin) < me->GetExactDist(nearest)))
                nearest = goblin;
        return nearest;
    }

    EventMap _events;
};

/*######
## 36615 Doc Zapnozzle (the wounded): patches up hurt goblins near him
######*/

enum DocZapnozzleField
{
    EVENT_DOC_FIRST_AID             = 1,
    SPELL_DOC_FIRST_AID             = 72996,
};

struct npc_lost_isles_doc_zapnozzle_field : public ScriptedAI
{
    npc_lost_isles_doc_zapnozzle_field(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _events.ScheduleEvent(EVENT_DOC_FIRST_AID, 8s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        // ArkCORE healed at two points of the Doc's walk, which TDB does not have: now every few seconds
        if (_events.ExecuteEvent() == EVENT_DOC_FIRST_AID)
        {
            std::list<Unit*> friends;
            Acore::AnyFriendlyUnitInObjectRangeCheck check(me, me, 15.0f);
            Acore::UnitListSearcher<Acore::AnyFriendlyUnitInObjectRangeCheck> searcher(me, friends, check);
            Cell::VisitObjects(me, searcher, 15.0f);
            for (Unit* unit : friends)
                if (unit->IsCreature() && unit->GetHealthPct() < 50.0f)
                {
                    DoCast(unit, SPELL_DOC_FIRST_AID, true);
                    break;
                }
            _events.ScheduleEvent(EVENT_DOC_FIRST_AID, 8s);
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
};

/*######
## 38855 Volcanoth: breathes fire in stage 7
######*/

enum Volcanoth
{
    EVENT_VOLCANOTH_FLAME_BREATH    = 1,
    EVENT_VOLCANOTH_VOLCANIC_BREATH,

    SPELL_COSMETIC_FLAME_BREATH     = 73016,
    SPELL_VOLCANIC_BREATH           = 73097,
};

struct npc_lost_isles_volcanoth : public ScriptedAI
{
    npc_lost_isles_volcanoth(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        if (me->HasPhase(PHASE_UP_THE_VOLCANO))
        {
            _events.ScheduleEvent(EVENT_VOLCANOTH_FLAME_BREATH, 5s);
            _events.ScheduleEvent(EVENT_VOLCANOTH_VOLCANIC_BREATH, 40s);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_VOLCANOTH_FLAME_BREATH:
                    DoCastSelf(SPELL_COSMETIC_FLAME_BREATH, true);
                    _events.ScheduleEvent(EVENT_VOLCANOTH_FLAME_BREATH, 35s, 45s);
                    break;
                case EVENT_VOLCANOTH_VOLCANIC_BREATH:
                    DoCastSelf(SPELL_VOLCANIC_BREATH, true);
                    _events.ScheduleEvent(EVENT_VOLCANOTH_VOLCANIC_BREATH, 4100ms, 7s);
                    break;
                default:
                    break;
            }
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
};

/*######
## Volcanoth! (24958) done: the volcano blows and Sassy calls the player to the bomber
######*/

enum VolcanothDone
{
    SPELL_DUMMY_TO_SASSY            = 73156,
    SPELL_IMMINENT_DEATH            = 73090,
    SPELL_SUMMON_DEAD_EXPLOSION_BUNNY = 73194,
    SPELL_VOLCANOTH_DEAD_EXPLOSION  = 73193,
    NPC_SASSY_HARDWRENCH_VOLCANO    = 38928,
    ACTION_SASSY_CALL_PLAYER        = 1,
};

class player_lost_isles_volcanoth : public PlayerScript
{
public:
    player_lost_isles_volcanoth() : PlayerScript("player_lost_isles_volcanoth",
        { PLAYERHOOK_ON_PLAYER_COMPLETE_QUEST }) { }

    void OnPlayerCompleteQuest(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() != QUEST_VOLCANOTH || player->IsQuestRewarded(QUEST_VOLCANOTH))
            return;

        player->CastSpell(player, SPELL_DUMMY_TO_SASSY, true);
        player->CastSpell(player, SPELL_IMMINENT_DEATH, true);
        player->CastSpell(player, SPELL_SUMMON_DEAD_EXPLOSION_BUNNY, true);
        // 73156 needs a spell condition for its nearby-entry target that TDB 434 does not have: Sassy is told directly
        if (Creature* sassy = player->FindNearestCreature(NPC_SASSY_HARDWRENCH_VOLCANO, 100.0f))
        {
            sassy->AI()->SetGUID(player->GetGUID(), DATA_PLAYER_GUID);
            sassy->AI()->DoAction(ACTION_SASSY_CALL_PLAYER);
        }
    }
};

/*######
## 38985 Volcanoth Dead Explosion Bunny
######*/

enum DeadExplosionBunny
{
    EVENT_DEAD_EXPLOSION            = 1,
};

struct npc_lost_isles_volcanoth_explosion_bunny : public ScriptedAI
{
    npc_lost_isles_volcanoth_explosion_bunny(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _events.ScheduleEvent(EVENT_DEAD_EXPLOSION, 500ms);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (_events.ExecuteEvent() == EVENT_DEAD_EXPLOSION)
            DoCastSelf(SPELL_VOLCANOTH_DEAD_EXPLOSION, true);
    }

private:
    EventMap _events;
};

/*######
## Stage 8 (phase 183) begins: 38928 Sassy Hardwrench and the flying bomber out of the volcano (Old Friends, 25023)
######*/

enum OldFriends
{
    EVENT_SASSY_CALL                = 1,

    SPELL_OLD_FRIENDS_ACCEPT_AND_BIND = 73135,
    SPELL_VOLCANOTH_EARTHQUAKE      = 72993,
    SPELL_SUMMON_FLYING_BOMBER      = 73105,

    SAY_SASSY_GET_OVER_HERE         = 0,
};

struct npc_lost_isles_sassy_hardwrench_volcano : public ScriptedAI
{
    npc_lost_isles_sassy_hardwrench_volcano(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _playerGUID.Clear();
    }

    void sQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() != QUEST_OLD_FRIENDS)
            return;

        _events.CancelEvent(EVENT_SASSY_CALL);
        DoCast(player, SPELL_OLD_FRIENDS_ACCEPT_AND_BIND, true);
        player->CastSpell(player, SPELL_VOLCANOTH_EARTHQUAKE, true);
        player->CastSpell(player, SPELL_SUMMON_FLYING_BOMBER, false);
    }

    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        if (id == DATA_PLAYER_GUID)
            _playerGUID = guid;
    }

    void DoAction(int32 action) override
    {
        if (action == ACTION_SASSY_CALL_PLAYER)
            _events.RescheduleEvent(EVENT_SASSY_CALL, 10ms);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (_events.ExecuteEvent() == EVENT_SASSY_CALL)
            if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
                if (player->GetDistance(me) > 30.0f && player->GetQuestStatus(QUEST_OLD_FRIENDS) == QUEST_STATUS_NONE)
                {
                    Talk(SAY_SASSY_GET_OVER_HERE, player);
                    _events.ScheduleEvent(EVENT_SASSY_CALL, 8s);
                }
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
};

/*######
## 38918 Flying Bomber: carries the player and Sassy (38869, its vehicle accessory) away from the exploding volcano
######*/

enum FlyingBomber
{
    EVENT_BOMBER_START              = 1,
    EVENT_BOMBER_FIREBALL,
    EVENT_BOMBER_ON_FIRE,
    EVENT_BOMBER_CHECK_ARRIVAL,
    EVENT_BOMBER_ENGINE_SOUND,

    SPELL_BOMBER_ROPE_LADDER        = 73149,
    SPELL_SUMMON_GIANT_FIREBALL     = 73127,
    SPELL_SASSY_YELL_02             = 73112,
    SPELL_SASSY_YELL_03             = 73185,
    SPELL_BOMBER_ON_FIRE            = 73191,
    SPELL_AREA_TRIGGER_BUNNY        = 73201, // triggers 73200, 73202, 73203
    SPELL_BOMBER_SMOKING            = 73192, // Old Friends: Flying Bomber's Smoking (after the fireball hits)
    SPELL_STOP_PLAYING_MUSIC        = 90247,

    SOUND_BOMBER_TAKE_OFF           = 23722,
    SOUND_BOMBER_FIREBALL           = 23723,
    SOUND_BOMBER_LANDING            = 23724,
    SOUND_BOMBER_ON_FIRE            = 23725,

    NPC_SASSY_HARDWRENCH_BOMBER     = 38869,
    SAY_BOMBER_SASSY_HANG_ON        = 0,
    SAY_BOMBER_SASSY_BEHIND_YOU     = 1, // Sassy Yell 02 (73112): "OH MY GOSH! BEHIND YOU!!!" (ArkCORE-NG's text)
    SAY_BOMBER_SASSY_ON_FIRE        = 2, // Sassy Yell 03 (73185): "WE'RE ON FIRE!" (ArkCORE-NG's text)
    POINT_BOMBER_LANDED             = 1,
};

Position const BomberFireballTarget = { 1072.83f, 1174.958f, 141.9254f, 0.0f };
// In front of Thrall (38935) on Warchief's Lookout, where Old Friends (25023) is handed in: TDB 434's quest POI puts the
// turn-in at his stage 5 spawn (1583, 2722), which stage 8 copies (own)
Position const BomberDestination = { 1579.0f, 2717.5f, 83.2f, 0.0f };

struct npc_lost_isles_flying_bomber : public VehicleAI
{
    npc_lost_isles_flying_bomber(Creature* creature) : VehicleAI(creature) { }

    void Reset() override
    {
        VehicleAI::Reset();
        if (!me->HasAura(SPELL_BOMBER_ROPE_LADDER))
            DoCastSelf(SPELL_BOMBER_ROPE_LADDER, true);
        _events.ScheduleEvent(EVENT_BOMBER_START, 2s);
    }

    void IsSummonedBy(WorldObject* summoner) override
    {
        if (Player* player = summoner ? summoner->ToPlayer() : nullptr)
        {
            _playerGUID = player->GetGUID();
            me->PlayDirectSound(SOUND_BOMBER_TAKE_OFF, player);
        }
    }

    void PassengerBoarded(Unit* passenger, int8 /*seatId*/, bool apply) override
    {
        if (!apply)
        {
            if (passenger->IsPlayer())
                me->DespawnOrUnsummon(10ms);
            return;
        }

        if (passenger->GetEntry() == NPC_SASSY_HARDWRENCH_BOMBER)
            _sassyGUID = passenger->GetGUID();
        else if (passenger->IsPlayer())
            _playerGUID = passenger->GetGUID();
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != EFFECT_MOTION_TYPE || id != POINT_BOMBER_LANDED)
            return;

        _sound = 0;
        if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
            player->CastSpell(player, SPELL_STOP_PLAYING_MUSIC, true);
        me->DespawnOrUnsummon(10ms);
    }

    void UpdateAI(uint32 diff) override
    {
        VehicleAI::UpdateAI(diff);
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID);
            switch (eventId)
            {
                case EVENT_BOMBER_START:
                    if (!player)
                        break;
                    if (Creature* sassy = ObjectAccessor::GetCreature(*me, _sassyGUID))
                        sassy->AI()->Talk(SAY_BOMBER_SASSY_HANG_ON, player);
                    me->SetCanFly(true);
                    me->SetDisableGravity(true);
                    me->SetSpeed(MOVE_FLIGHT, 3.5f);
                    me->SetSpeed(MOVE_RUN, 3.5f);
                    // ArkCORE's 26-point flight is not available; its waypoint events now follow the clock
                    MoveFlight(me, POINT_BOMBER_LANDED, BomberDestination, 60.0f);
                    _events.ScheduleEvent(EVENT_BOMBER_FIREBALL, 4s);
                    _events.ScheduleEvent(EVENT_BOMBER_ON_FIRE, 12s);
                    _events.ScheduleEvent(EVENT_BOMBER_CHECK_ARRIVAL, 1s);
                    break;
                case EVENT_BOMBER_FIREBALL:
                    me->CastSpell(BomberFireballTarget.GetPositionX(), BomberFireballTarget.GetPositionY(),
                        BomberFireballTarget.GetPositionZ(), SPELL_SUMMON_GIANT_FIREBALL, true);
                    if (player)
                        player->CastSpell(player, SPELL_SASSY_YELL_02, true);
                    DoCastSelf(SPELL_BOMBER_SMOKING, true);
                    _sound = SOUND_BOMBER_FIREBALL;
                    _events.ScheduleEvent(EVENT_BOMBER_ENGINE_SOUND, 1s);
                    break;
                case EVENT_BOMBER_ON_FIRE:
                    if (player)
                    {
                        player->CastSpell(player, SPELL_SASSY_YELL_03, true);
                        player->CastSpell(player, SPELL_BOMBER_ON_FIRE, true);
                    }
                    _sound = SOUND_BOMBER_ON_FIRE;
                    break;
                case EVENT_BOMBER_CHECK_ARRIVAL:
                    // ArkCORE's waypoint 22 of 26: the landing begins
                    if (me->GetExactDist2d(BomberDestination.GetPositionX(), BomberDestination.GetPositionY()) < 150.0f)
                    {
                        if (player)
                            player->CastSpell(player, SPELL_AREA_TRIGGER_BUNNY, true);
                        _sound = SOUND_BOMBER_LANDING;
                        break;
                    }
                    _events.ScheduleEvent(EVENT_BOMBER_CHECK_ARRIVAL, 1s);
                    break;
                case EVENT_BOMBER_ENGINE_SOUND:
                    if (player && _sound)
                        me->PlayDirectSound(_sound, player);
                    if (_sound)
                        _events.ScheduleEvent(EVENT_BOMBER_ENGINE_SOUND, 1s);
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
    ObjectGuid _sassyGUID;
    uint32 _sound = 0;
};

/*######
## Old Friends: Sassy Yell 02 (73112) and 03 (73185) hit the player; Sassy, who rides the same bomber, says the line
## (the client's broadcast texts, as ArkCORE-NG's creature_text pairs them). Yells 04 to 08 have no known text.
######*/

class spell_lost_isles_old_friends_sassy_yell : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_old_friends_sassy_yell);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetHitUnit() ? GetHitUnit()->ToPlayer() : nullptr;
        Unit* bomber = player ? player->GetVehicleBase() : nullptr;
        Vehicle* vehicle = bomber ? bomber->GetVehicleKit() : nullptr;
        if (!vehicle)
            return;

        uint8 const group = GetSpellInfo()->Id == SPELL_SASSY_YELL_02 ? SAY_BOMBER_SASSY_BEHIND_YOU : SAY_BOMBER_SASSY_ON_FIRE;
        for (auto const& [seat, info] : vehicle->Seats)
            if (Unit* passenger = ObjectAccessor::GetUnit(*bomber, info.Passenger.Guid))
                if (passenger->GetEntry() == NPC_SASSY_HARDWRENCH_BOMBER)
                    if (Creature* sassy = passenger->ToCreature())
                        sassy->AI()->Talk(group, player);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_lost_isles_old_friends_sassy_yell::HandleScript, EFFECT_0,
            SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

/*######
## 38979 Giant Fireball Bunny: the volcano's fireball that hits the bomber
######*/

enum GiantFireball
{
    EVENT_FIREBALL_EXPLODE          = 1,
    SPELL_GIANT_FIREBALL            = 73111, // triggers 73181
    SPELL_GIANT_FIREBALL_IMPACT     = 73182,
};

struct npc_lost_isles_giant_fireball_bunny : public ScriptedAI
{
    npc_lost_isles_giant_fireball_bunny(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        // ArkCORE flew the fireball along its own path and burst at the first point
        _events.Reset();
        _events.ScheduleEvent(EVENT_FIREBALL_EXPLODE, 2s);
        me->DespawnOrUnsummon(15s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (_events.ExecuteEvent() == EVENT_FIREBALL_EXPLODE)
        {
            DoCastSelf(SPELL_GIANT_FIREBALL, true);
            DoCastSelf(SPELL_GIANT_FIREBALL_IMPACT, true);
        }
    }

private:
    EventMap _events;
};

/*######
## Stages 8-10: MaelstromCore's own scripts, no ArkCORE source
##
## Written from the 4.3.4 client's spells (Spell.dbc, SpellEffect.dbc) and TDB 434's data. What TDB's data does by
## itself is left to it: the kill and loot quests, the valves (goobers), the Big Red Button (73892 forces 73900, which
## gives the Good-bye, Sweet Oil credit), the reward spell of Kaja'Cola Gives You IDEAS! (73604, which summons
## Assistant Greely 39199 as a companion) and the spell clicks of the captured goblins (73947) and the Ultimate
## Footbomb Uniform (73991). The rides start from their quests' SourceSpellID (build_world_db.py goblin()).
######*/

enum LostIslesLateStages
{
    NPC_GOBLIN_SURVIVOR_MINE        = 38409,
    NPC_ACE_MINE                    = 38441,
    NPC_IZZY_MINE                   = 38647,
    NPC_GOBBER_MINE                 = 38746,
    NPC_SOULSTONE_CREDIT            = 39276, // Throw It On The Ground! Kill Credit
    NPC_MINE_CART_RIDE_CREDIT       = 39335, // Wild Mine Cart Ride Kill Credit
    NPC_CAPTURED_GOBLIN             = 39456,
    NPC_TRADE_PRINCE_GALLYWIX_DOCKS = 39582,

    SPELL_INTIMIDATED               = 73617, // the mine's goblins, until they drink Kaja'Cola Zero-One
    SPELL_ESCAPE_VELOCITY_ROCKETS   = 73948,
    SPELL_UNLOAD_TOXIC_ASSETS       = 74000,
    SPELL_DOWNSIZING                = 74003,
    SPELL_REVENUE_STREAM            = 74005,

    SAY_SURVIVOR_IDEA_FIRST         = 5,     // 38409's Kaja'Cola ideas, groups 5-12
    SAY_SURVIVOR_IDEA_LAST          = 12,
    SAY_CAPTURED_GOBLIN_FIRST       = 0,     // 39456's lines, groups 0-5
    SAY_CAPTURED_GOBLIN_LAST        = 5,

    POINT_LATE_RIDE_END             = 3001,
    POINT_CAPTURED_GOBLIN_LAUNCH    = 3002,
    EVENT_LATE_RIDE_START           = 1,
    EVENT_LATE_RIDE_TIMEOUT,
    LATE_RIDE_MAX_LEGS              = 10,
};

/*######
## 38318 Mechashark X-Steam: A Goblin in Shark's Clothing (24817). A Mechashark X-Steam Controller (202108) casts
## 71662, which makes the player cast 71648: the shark is summoned as the player's vehicle (SummonProperties 827) and
## they fight The Hammer (36682) with its three spells. The shark carries its visual (71663). It belongs to the Dire
## Strait: outside it the player is warned with the client's own line (broadcast text 38282, the warning aura 71749
## shows it on the buff bar), and still outside after the grace time the shark falls apart and puts them down. No
## source has the shark's script; the spells and the line are the client's.
######*/

enum MechasharkXSteam
{
    AREA_DIRE_STRAIT                    = 4816,
    SPELL_MECHASHARK_VISUAL             = 71663, // A Goblin In Shark's Clothing: Mechashark X-Steam's Visual Aura
    SPELL_LEAVING_DIRE_STRAIT_WARNING   = 71749,
    TEXT_LEAVING_DIRE_STRAIT            = 38282, // "Return to the Dire Strait or the Mechashark X-Steam will ..."
    EVENT_MECHASHARK_CHECK_AREA         = 1,
};

constexpr float MECHASHARK_FREE_RANGE = 80.0f; // yards from where it was summoned that never count as leaving
constexpr Seconds MECHASHARK_GRACE = 10s;

struct npc_lost_isles_mechashark : public VehicleAI
{
    npc_lost_isles_mechashark(Creature* creature) : VehicleAI(creature) { }

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        DoCastSelf(SPELL_MECHASHARK_VISUAL, true);
    }

    void PassengerBoarded(Unit* passenger, int8 /*seatId*/, bool apply) override
    {
        Player* player = passenger->ToPlayer();
        if (!player)
            return;

        if (apply)
        {
            _playerGUID = player->GetGUID();
            _home = me->GetPosition(); // the player boards where the controller summoned the shark
            _events.ScheduleEvent(EVENT_MECHASHARK_CHECK_AREA, 1s);
        }
        else
        {
            player->RemoveAurasDueToSpell(SPELL_LEAVING_DIRE_STRAIT_WARNING);
            _events.Reset();
            me->DespawnOrUnsummon(1s);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        VehicleAI::UpdateAI(diff);
        _events.Update(diff);

        if (_events.ExecuteEvent() != EVENT_MECHASHARK_CHECK_AREA)
            return;

        Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID);
        if (!player || player->GetVehicleBase() != me)
            return;

        bool const outside = me->GetAreaId() != AREA_DIRE_STRAIT && me->GetExactDist2d(&_home) > MECHASHARK_FREE_RANGE;
        if (!outside)
        {
            _outsideFor = 0s;
            player->RemoveAurasDueToSpell(SPELL_LEAVING_DIRE_STRAIT_WARNING);
        }
        else if (_outsideFor == 0s)
        {
            me->Whisper(TEXT_LEAVING_DIRE_STRAIT, player, true);
            player->CastSpell(player, SPELL_LEAVING_DIRE_STRAIT_WARNING, true);
            _outsideFor = 1s;
        }
        else if ((_outsideFor += 1s) > MECHASHARK_GRACE)
        {
            if (Vehicle* vehicle = me->GetVehicleKit())
                vehicle->RemoveAllPassengers(); // PassengerBoarded(false) despawns the shark
            return;
        }

        _events.ScheduleEvent(EVENT_MECHASHARK_CHECK_AREA, 1s);
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
    Position _home;
    Seconds _outsideFor = 0s;
};

/*######
## 39074 Pride of Kezan: The Pride of Kezan (25066). Sassy Hardwrench at Warchief's Lookout gives it; accepting it, or
## her "I am ready." while it is under way, makes the player cast 73431, whose 73430 summons the bomber as their vehicle
## (SummonProperties 827). The player flies it (Flight Speed Aura 73446) and shoots down the Gnomeregan Stealth Fighters
## (39039) with its two spells. It smokes (73490) and then burns (73493) as it takes damage. Flown away from the fight it
## shows the leaving-area warning (73428); still away after the grace time, the player bails out on a parachute (73455).
## No source has its script; the spells are the client's.
######*/

enum PrideOfKezan
{
    SPELL_PRIDE_FLIGHT_SPEED            = 73446,
    SPELL_PRIDE_LEAVING_AREA_WARNING    = 73428,
    SPELL_PRIDE_PARACHUTE_MASTER        = 73455,
    SPELL_PRIDE_SMOKING                 = 73490,
    SPELL_PRIDE_ON_FIRE                 = 73493,
    SPELL_STEALTH_FIGHTER_ON_FIRE       = 73481,
    EVENT_PRIDE_CHECK_AREA              = 1,
};

// The middle of the Gnomeregan Stealth Fighters' sky over Warchief's Lookout and the Tranquil Coast, and how far from it
// the bomber may fly (the fighters span some 500 yards)
Position const PrideOfKezanSky = { 1862.0f, 2973.0f, 100.0f, 0.0f };
constexpr float PRIDE_RANGE = 450.0f;
constexpr Seconds PRIDE_GRACE = 10s;

struct npc_lost_isles_pride_of_kezan : public VehicleAI
{
    npc_lost_isles_pride_of_kezan(Creature* creature) : VehicleAI(creature) { }

    void PassengerBoarded(Unit* passenger, int8 /*seatId*/, bool apply) override
    {
        Player* player = passenger->ToPlayer();
        if (!player)
            return;

        if (apply)
        {
            _playerGUID = player->GetGUID();
            me->SetCanFly(true);
            me->SetDisableGravity(true);
            DoCastSelf(SPELL_PRIDE_FLIGHT_SPEED, true);
            _events.ScheduleEvent(EVENT_PRIDE_CHECK_AREA, 1s);
        }
        else
        {
            player->RemoveAurasDueToSpell(SPELL_PRIDE_LEAVING_AREA_WARNING);
            _events.Reset();
            me->DespawnOrUnsummon(1s);
        }
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*type*/, SpellSchoolMask /*school*/) override
    {
        float const pct = (float(me->GetHealth()) - float(damage)) * 100.0f / float(me->GetMaxHealth());
        if (pct < 60.0f && !me->HasAura(SPELL_PRIDE_SMOKING))
            DoCastSelf(SPELL_PRIDE_SMOKING, true);
        if (pct < 30.0f && !me->HasAura(SPELL_PRIDE_ON_FIRE))
            DoCastSelf(SPELL_PRIDE_ON_FIRE, true);
    }

    void UpdateAI(uint32 diff) override
    {
        VehicleAI::UpdateAI(diff);
        _events.Update(diff);

        if (_events.ExecuteEvent() != EVENT_PRIDE_CHECK_AREA)
            return;

        Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID);
        if (!player || player->GetVehicleBase() != me)
            return;

        if (me->GetExactDist2d(&PrideOfKezanSky) <= PRIDE_RANGE)
        {
            _awayFor = 0s;
            player->RemoveAurasDueToSpell(SPELL_PRIDE_LEAVING_AREA_WARNING);
        }
        else if (_awayFor == 0s)
        {
            player->CastSpell(player, SPELL_PRIDE_LEAVING_AREA_WARNING, true);
            _awayFor = 1s;
        }
        else if ((_awayFor += 1s) > PRIDE_GRACE)
        {
            // The parachute's master spell takes the player out of the seat; PassengerBoarded(false) removes the bomber
            player->CastSpell(player, SPELL_PRIDE_PARACHUTE_MASTER, true);
            if (player->GetVehicleBase() == me)
                if (Vehicle* vehicle = me->GetVehicleKit())
                    vehicle->RemoveAllPassengers();
            return;
        }

        _events.ScheduleEvent(EVENT_PRIDE_CHECK_AREA, 1s);
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
    Seconds _awayFor = 0s;
};

/*######
## 39039 Gnomeregan Stealth Fighter: catches fire (73481) when the Pride of Kezan has shot it half down
######*/

struct npc_lost_isles_stealth_fighter : public VehicleAI
{
    npc_lost_isles_stealth_fighter(Creature* creature) : VehicleAI(creature) { }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*type*/, SpellSchoolMask /*school*/) override
    {
        if (damage < me->GetHealth() && (me->GetHealth() - damage) * 2 < me->GetMaxHealth()
            && !me->HasAura(SPELL_STEALTH_FIGHTER_ON_FIRE))
            DoCastSelf(SPELL_STEALTH_FIGHTER_ON_FIRE, true);
    }
};

/*######
## Zombies vs. Super Booster Rocket Boots (24942): the boots (item 52013) cast 72891, whose script effect makes the
## player put them on: 72889 summons the Super Booster Rocket Boots (38802) as their vehicle (SummonProperties 827).
## Boarded, the boots run their blast (72885, the "AICast on Enter Seat" 72897's aura), which every half second hits
## the zombies around (72886, credit through npc_lost_isles_goblin_zombie). Leaving them takes them off.
######*/

enum SuperBoosterRocketBoots
{
    SPELL_ROCKET_BOOTS_SUMMON           = 72889,
    SPELL_ROCKET_BOOTS_DAMAGE_TRIGGER   = 72885,
};

class spell_lost_isles_rocket_boots_item : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_rocket_boots_item);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        if (Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr)
            if (!player->GetVehicle())
                player->CastSpell(player, SPELL_ROCKET_BOOTS_SUMMON, true);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_lost_isles_rocket_boots_item::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

struct npc_lost_isles_rocket_boots : public VehicleAI
{
    npc_lost_isles_rocket_boots(Creature* creature) : VehicleAI(creature) { }

    void PassengerBoarded(Unit* passenger, int8 /*seatId*/, bool apply) override
    {
        if (!passenger->IsPlayer())
            return;

        if (apply)
            DoCastSelf(SPELL_ROCKET_BOOTS_DAMAGE_TRIGGER, true);
        else
        {
            me->RemoveAurasDueToSpell(SPELL_ROCKET_BOOTS_DAMAGE_TRIGGER);
            me->DespawnOrUnsummon(500ms);
        }
    }
};

/*######
## 39194 Blastshadow the Brutemaster: Throw It On the Ground! (25123). His soulstone chest (202574) appears where he
## falls (73703's object; it had three spawns that could be looted with him alive) and his Delicia Whipsnaps (39195)
## leaves with him, back when he respawns.
######*/

enum Blastshadow
{
    NPC_DELICIA_WHIPSNAPS               = 39195,
    GO_BLASTSHADOWS_SOULSTONE           = 202574,
};

struct npc_lost_isles_blastshadow : public ScriptedAI
{
    npc_lost_isles_blastshadow(Creature* creature) : ScriptedAI(creature) { }

    void JustDied(Unit* /*killer*/) override
    {
        Seconds const respawn = Seconds(me->GetRespawnDelay());
        me->SummonGameObject(GO_BLASTSHADOWS_SOULSTONE, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(),
            me->GetOrientation(), 0.0f, 0.0f, 0.0f, 0.0f, 120);
        if (Creature* delicia = me->FindNearestCreature(NPC_DELICIA_WHIPSNAPS, 40.0f))
            delicia->DespawnOrUnsummon(1s, respawn);
    }
};

/*######
## It's a Town-In-A-Box (14245): the plunger (201938, 68938) deploys the town; four smoke bunnies (71093, 71095,
## 71096, 71097: 38069 with the ForgeArms smoke 71094) puff up around it. The bunnies' spots are ours (no source has
## them); the spells and auras are the client's (ArkCORE-NG's addon gives 38069 its auras).
######*/

std::array<uint32, 4> const TownSmokeSpells = { 71093, 71095, 71096, 71097 };

class spell_lost_isles_town_in_a_box_plunger : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_town_in_a_box_plunger);

    void HandleAfterCast()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        for (uint8 i = 0; i < TownSmokeSpells.size(); ++i)
        {
            float const angle = caster->GetOrientation() + float(M_PI) / 4.0f + i * float(M_PI) / 2.0f;
            float const x = caster->GetPositionX() + 12.0f * std::cos(angle);
            float const y = caster->GetPositionY() + 12.0f * std::sin(angle);
            caster->CastSpell(x, y, caster->GetMapHeight(x, y, caster->GetPositionZ() + 5.0f), TownSmokeSpells[i], true);
        }
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_lost_isles_town_in_a_box_plunger::HandleAfterCast);
    }
};

/*######
## Warchief's Revenge (14243): the Rope Ladders (196396, 196397) by the Alliance ship cast 68838 / 68840 on the
## swimmer, whose script effects lead to Rope Ladder A / B (68488 / 68489, jumps). No source has where the jumps land;
## the player climbs onto the lower deck by each ladder, where the sailors stand (ours).
######*/

Position const RopeLadderDeckA = { 970.5f, 3817.0f, 3.4f, 0.0f }; // by ladder 196396
Position const RopeLadderDeckB = { 983.5f, 3812.0f, 3.4f, 0.0f }; // by ladder 196397

class spell_lost_isles_rope_ladder : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_rope_ladder);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetHitUnit() ? GetHitUnit()->ToPlayer() : nullptr;
        if (!player)
            return;

        Position const& deck = GetSpellInfo()->Id == 68838 ? RopeLadderDeckA : RopeLadderDeckB;
        player->GetMotionMaster()->MoveJump(deck, 10.0f, 12.0f);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_lost_isles_rope_ladder::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

/*######
## Gossip options no source has, for quest items or vehicles a player can lose: the quest givers' own requests (the
## client's broadcast texts, options from the builder's LOST_ISLES_GOSSIP_OPTIONS) cast the quest's own spell.
######*/

struct LostIslesGossipSpell
{
    uint32 menu;
    uint32 option;
    uint32 quest;
    uint32 spell;
};

std::array<LostIslesGossipSpell, 7> const LostIslesGossipSpells =
{{
    { 10823, 0, 14021, 68069 }, // Dampwick: "Er, Dampwick, I need another miner...." (New Miner from Gossip)
    { 10708, 0, 14238, 68346 }, // Kilag: "Kilag, I... uh, somehow... er, lost my scout." (New Orc Scout from Gossip)
    { 10708, 1, 14240, 68973 }, // Kilag: "I need to borrow your tamed jungle cat..." (To the Cliffs: Bastia)
    { 10965, 0, 24671, 74172 }, // Hobart: "I seem to have misplaced the remote control fireworks."
    { 10965, 1, 25214, 73967 }, // Hobart: "Hobart, I need more single-stage booster rockets!"
    { 11074, 0, 24942, 72948 }, // Coach Crosscheck: "Coach, can I get some rocket boots?"
    { 12582, 0, 25066, 73431 }, // Sassy at the Lookout: "I am ready." (The Pride of Kezan's bomber)
}};

bool HandleLostIslesGossipSpell(Player* player, uint32 menu, uint32 option)
{
    for (LostIslesGossipSpell const& entry : LostIslesGossipSpells)
    {
        if (entry.menu != menu || entry.option != option)
            continue;

        CloseGossipMenuFor(player);
        if (player->GetQuestStatus(entry.quest) == QUEST_STATUS_INCOMPLETE && !player->GetVehicle())
            player->CastSpell(player, entry.spell, true);
        return true;
    }
    return false;
}

/*######
## 36719 Pterrordax Scavenger: circles in the sky over its spot like a vulture. TDB 434 only lets the flying ones drift
## 5-10 yards about their spot, which in the air looks like hovering; all of them circle (the user, 2026-11-10). Each
## circles on its own radius and in its own direction (from its spawn), so a flock does not wheel in step.
######*/

struct npc_lost_isles_pterrordax_scavenger : public ScriptedAI
{
    npc_lost_isles_pterrordax_scavenger(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _circling = false;
    }

    void JustReachedHome() override
    {
        _circling = false;
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!UpdateVictim())
        {
            // Started here, not in Reset: the creature's default movement is set up around Reset and replaced it
            if (!_circling && !me->IsInEvadeMode())
                Circle();
            return;
        }

        _circling = false;
        DoMeleeAttackIfReady();
    }

private:
    bool _circling = false;

    void Circle()
    {
        _circling = true;
        uint32 const seed = me->GetSpawnId();
        float const radius = 15.0f + float(seed % 21);
        Position const& home = me->GetHomePosition();
        me->GetMotionMaster()->Clear();
        // every one soars over its spot, whatever its spawn's movement; flown even if the spawn lacks the fly flag
        me->GetMotionMaster()->MoveCirclePath(home.GetPositionX(), home.GetPositionY(), home.GetPositionZ(), radius,
            seed % 2 == 0, 16, FORCED_MOVEMENT_FLY);
    }
};

/*######
## 43359 ELM General Purpose Bunny Infinite Hide Body, the Lost Isles' (stage 8 on): the erupted volcano throws fiery
## boulders. While a player is near, each one casts one of the client's four "Volcano: Fiery Boulder" spells (74070,
## 74072, 74076, 74085) at the ground nearby every few seconds. ArkCORE-NG wrote this and left it switched off (its
## core could not run it); the spells are the client's, the spots and timing ours. Bound per spawn: Kezan's 43359 are
## the bombardment's anchors (zone_kezan.cpp).
######*/

enum VolcanoBoulders
{
    EVENT_VOLCANO_BOULDER           = 1,
};

std::array<uint32, 4> const VolcanoBoulderSpells = { 74070, 74072, 74076, 74085 };
constexpr float VOLCANO_BOULDER_PLAYER_RANGE = 150.0f;

struct npc_lost_isles_volcano_boulder_bunny : public ScriptedAI
{
    npc_lost_isles_volcano_boulder_bunny(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        me->SetReactState(REACT_PASSIVE);
        _events.Reset();
        _events.ScheduleEvent(EVENT_VOLCANO_BOULDER, randtime(1s, 6s));
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);
        if (_events.ExecuteEvent() != EVENT_VOLCANO_BOULDER)
            return;

        _events.ScheduleEvent(EVENT_VOLCANO_BOULDER, randtime(4s, 6s));
        if (!me->SelectNearestPlayer(VOLCANO_BOULDER_PLAYER_RANGE))
            return;

        float const angle = frand(0.0f, 2.0f * float(M_PI));
        float const distance = frand(10.0f, 35.0f);
        float x = me->GetPositionX() + distance * std::cos(angle);
        float y = me->GetPositionY() + distance * std::sin(angle);
        float z = me->GetMapHeight(x, y, me->GetPositionZ() + 30.0f);
        if (z <= INVALID_HEIGHT)
            z = me->GetPositionZ();
        me->CastSpell(x, y, z, Acore::Containers::SelectRandomContainerElement(VolcanoBoulderSpells), true);
    }

private:
    EventMap _events;
};

/*######
## Good-bye, Sweet Oil (25207): the Big Red Button (205061) makes the player cast the Explosion Check (73900, the
## credit). Then they summon the Explosion Controller Bunny (73888, 39383), which sets off the platform's three
## explosions (73904, 73890, 73891) and shakes the player's camera (73905). The order and spells are the client's; the
## timing is ours (no source has it).
######*/

enum GoodbyeSweetOil
{
    SPELL_SWEET_OIL_EXPLOSION_CHECK     = 73900,
    SPELL_SWEET_OIL_SUMMON_CONTROLLER   = 73888,
    SPELL_SWEET_OIL_EXPLOSIONS_01       = 73904,
    SPELL_SWEET_OIL_EXPLOSIONS_02       = 73890,
    SPELL_SWEET_OIL_EXPLOSIONS_03       = 73891,
    SPELL_SWEET_OIL_CAMERA_SHAKE        = 73905,
    EVENT_SWEET_OIL_EXPLOSIONS_01       = 1,
    EVENT_SWEET_OIL_EXPLOSIONS_02,
    EVENT_SWEET_OIL_EXPLOSIONS_03,
};

class spell_lost_isles_sweet_oil_explosion_check : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_sweet_oil_explosion_check);

    void HandleAfterCast()
    {
        if (Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr)
            player->CastSpell(player, SPELL_SWEET_OIL_SUMMON_CONTROLLER, true);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_lost_isles_sweet_oil_explosion_check::HandleAfterCast);
    }
};

struct npc_lost_isles_sweet_oil_controller : public ScriptedAI
{
    npc_lost_isles_sweet_oil_controller(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        if (Player* player = summoner ? summoner->ToPlayer() : nullptr)
            _playerGUID = player->GetGUID();
        me->SetReactState(REACT_PASSIVE);
        _events.ScheduleEvent(EVENT_SWEET_OIL_EXPLOSIONS_01, 500ms);
        _events.ScheduleEvent(EVENT_SWEET_OIL_EXPLOSIONS_02, 2500ms);
        _events.ScheduleEvent(EVENT_SWEET_OIL_EXPLOSIONS_03, 4500ms);
        me->DespawnOrUnsummon(10s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);
        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_SWEET_OIL_EXPLOSIONS_01:
                    DoCastSelf(SPELL_SWEET_OIL_EXPLOSIONS_01, true);
                    break;
                case EVENT_SWEET_OIL_EXPLOSIONS_02:
                    DoCastSelf(SPELL_SWEET_OIL_EXPLOSIONS_02, true);
                    break;
                case EVENT_SWEET_OIL_EXPLOSIONS_03:
                    DoCastSelf(SPELL_SWEET_OIL_EXPLOSIONS_03, true);
                    break;
                default:
                    break;
            }
            if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
                me->CastSpell(player, SPELL_SWEET_OIL_CAMERA_SHAKE, true);
        }
    }

private:
    EventMap _events;
    ObjectGuid _playerGUID;
};

/*######
## The ground rides of stages 8 and 9: no source has their paths, so they go to the next quest NPC with mmaps path finding
## (a path that stops short is continued, as in the earlier stages). A ride that does not get there within its legs or
## its time puts the player down at the destination.
######*/

struct npc_lost_isles_late_ride : public VehicleAI
{
    npc_lost_isles_late_ride(Creature* creature, Position const& destination, float speed, Seconds timeout)
        : VehicleAI(creature), _destination(destination), _speed(speed), _timeout(timeout) { }

    void PassengerBoarded(Unit* passenger, int8 /*seatId*/, bool apply) override
    {
        Player* player = passenger->ToPlayer();
        if (!player)
            return;

        if (apply)
        {
            _playerGUID = player->GetGUID();
            _events.ScheduleEvent(EVENT_LATE_RIDE_START, 2s);
            _events.ScheduleEvent(EVENT_LATE_RIDE_TIMEOUT, _timeout);
        }
        else if (!_arrived)
            me->DespawnOrUnsummon(1s); // the player got off on the way
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE || id != POINT_LATE_RIDE_END || _arrived)
            return;

        if (me->GetExactDist2d(_destination) > 10.0f && ++_legs < LATE_RIDE_MAX_LEGS)
        {
            MoveRide(me, POINT_LATE_RIDE_END, _destination, false);
            return;
        }

        EndRide();
    }

    void UpdateAI(uint32 diff) override
    {
        VehicleAI::UpdateAI(diff);
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_LATE_RIDE_START:
                    me->SetWalk(false);
                    me->SetSpeed(MOVE_RUN, _speed);
                    MoveRide(me, POINT_LATE_RIDE_END, _destination, false);
                    break;
                case EVENT_LATE_RIDE_TIMEOUT:
                    if (!_arrived)
                        EndRide();
                    break;
                default:
                    break;
            }
        }
    }

protected:
    // The player is off the vehicle when this is called
    virtual void RideEnded(Player* /*player*/) { }

private:
    void EndRide()
    {
        _arrived = true;
        _events.Reset();
        bool const there = me->GetExactDist2d(_destination) <= 10.0f;
        if (Vehicle* vehicle = me->GetVehicleKit())
            vehicle->RemoveAllPassengers();

        if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
        {
            if (!there)
                player->NearTeleportTo(_destination.GetPositionX(), _destination.GetPositionY(),
                    _destination.GetPositionZ(), player->GetOrientation());
            RideEnded(player);
        }
        me->DespawnOrUnsummon(1s);
    }

    EventMap _events;
    ObjectGuid _playerGUID;
    Position const _destination;
    float const _speed;
    Seconds const _timeout;
    uint8 _legs = 0;
    bool _arrived = false;
};

/*######
## 39152 Bastia: Let's Ride (25100) carries the player from Kilag Gorefang to Slinky Sharpshiv at the labor mine. The
## quest's SourceSpellID 73532 summons Bastia and seats the player (73531).
######*/

Position const BastiaMineDestination = { 1823.0f, 1960.0f, 222.0f, 0.0f }; // beside Slinky Sharpshiv (38517, stage 8)

/*######
## 39044 Orc Battlesworn: Repel the Paratroopers (25024). The Alliance's planes drop their paratroopers (39042) onto
## the coast where the battlesworn hold the ramps; no source has where or how often. While a player is near, each
## battlesworn has one paratrooper of its own come down above it on the client's parachute (73363) and fight it.
######*/

enum RepelTheParatroopers
{
    NPC_ALLIANCE_PARATROOPER        = 39042,
    SPELL_PARACHUTE_VISUAL          = 73363, // Cosmetic - Parachute (Visual Only - Underwater Cancel)
    EVENT_PARATROOPER_DROP          = 1,
};

struct npc_lost_isles_orc_battlesworn : public ScriptedAI
{
    npc_lost_isles_orc_battlesworn(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _events.ScheduleEvent(EVENT_PARATROOPER_DROP, randtime(10s, 40s));
    }

    void SummonedCreatureDies(Creature* summon, Unit* /*killer*/) override
    {
        if (summon->GetGUID() == _paratrooper)
            _paratrooper.Clear();
    }

    void SummonedCreatureDespawn(Creature* summon) override
    {
        if (summon->GetGUID() == _paratrooper)
            _paratrooper.Clear();
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);
        if (_events.ExecuteEvent() == EVENT_PARATROOPER_DROP)
        {
            if (_paratrooper.IsEmpty() && me->SelectNearestPlayer(60.0f))
                DropParatrooper();
            _events.ScheduleEvent(EVENT_PARATROOPER_DROP, randtime(30s, 50s));
        }

        if (UpdateVictim())
            DoMeleeAttackIfReady();
    }

private:
    void DropParatrooper()
    {
        Position landing = me->GetNearPosition(frand(4.0f, 8.0f), frand(0.0f, 2.0f * float(M_PI)));
        Position drop(landing.GetPositionX(), landing.GetPositionY(), landing.GetPositionZ() + 20.0f,
            landing.GetAngle(me));
        Creature* paratrooper = me->SummonCreature(NPC_ALLIANCE_PARATROOPER, drop, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 15000);
        if (!paratrooper)
            return;

        _paratrooper = paratrooper->GetGUID();
        paratrooper->CastSpell(paratrooper, SPELL_PARACHUTE_VISUAL, true);
        paratrooper->SetCanFly(false);
        paratrooper->GetMotionMaster()->MoveFall();
        paratrooper->AI()->AttackStart(me);
    }

    EventMap _events;
    ObjectGuid _paratrooper;
};

struct npc_lost_isles_bastia_mine : public npc_lost_isles_late_ride
{
    npc_lost_isles_bastia_mine(Creature* creature) : npc_lost_isles_late_ride(creature, BastiaMineDestination, 2.0f, 150s)
    { }
};

/*######
## 73583 Kaja'Cola Zero-One (Morale Boost, 25122): frees the mind of an intimidated goblin in the labor mine. Ace, Izzy
## and Gobber count once each and stay for the next player; a freed survivor has an idea and leaves (respawns later).
######*/

bool IsMoraleBoostTarget(uint32 entry)
{
    return entry == NPC_GOBLIN_SURVIVOR_MINE || entry == NPC_ACE_MINE || entry == NPC_IZZY_MINE
        || entry == NPC_GOBBER_MINE;
}

class spell_lost_isles_kaja_cola_zero_one : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_kaja_cola_zero_one);

    SpellCastResult CheckTarget()
    {
        Unit* target = GetExplTargetUnit();
        if (!target || !IsMoraleBoostTarget(target->GetEntry()) || !target->HasAura(SPELL_INTIMIDATED))
            return SPELL_FAILED_BAD_TARGETS;
        return SPELL_CAST_OK;
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetCaster()->ToPlayer();
        Creature* target = GetHitCreature();
        if (!player || !target || !target->HasAura(SPELL_INTIMIDATED))
            return;

        player->KilledMonsterCredit(target->GetEntry());
        if (target->GetEntry() != NPC_GOBLIN_SURVIVOR_MINE)
            return;

        target->RemoveAurasDueToSpell(SPELL_INTIMIDATED);
        target->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
        target->AI()->Talk(urand(SAY_SURVIVOR_IDEA_FIRST, SAY_SURVIVOR_IDEA_LAST), player);
        target->DespawnOrUnsummon(5s);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_lost_isles_kaja_cola_zero_one::CheckTarget);
        OnEffectHitTarget += SpellEffectFn(spell_lost_isles_kaja_cola_zero_one::HandleDummy, EFFECT_0,
            SPELL_EFFECT_DUMMY);
    }
};

/*######
## 69040 Don't Go Into The Light!: Initial Placement Teleport (14239): the master spell's teleport onto the beach; its
## script effect lays the player low: Near Death! (stunned and hidden), dead on the sand at 1 health. Their Doc follows.
######*/

class spell_lost_isles_dgitl_placement : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_dgitl_placement);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetHitPlayer();
        if (!player)
            return;

        if (!player->HasAura(SPELL_NEAR_DEATH))
            player->CastSpell(player, SPELL_NEAR_DEATH, true);
        if (!player->HasAura(SPELL_COSMETIC_STATE_DEATH))
            player->AddAura(SPELL_COSMETIC_STATE_DEATH, player);
        player->SetHealth(1);
        ScheduleDocSummon(player, 3s);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_lost_isles_dgitl_placement::HandleScript, EFFECT_1,
            SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

/*######
## 69018 Don't Go Into The Light!: Summon Doc Zapnozzle (14239): he comes up out of the sea, 20 yards out
######*/

class spell_lost_isles_dgitl_summon_doc : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_dgitl_summon_doc);

    void SetDest(SpellDestination& dest)
    {
        dest.Relocate(DocSummonPos);
    }

    void Register() override
    {
        OnDestinationTargetSelect += SpellDestinationTargetSelectFn(spell_lost_isles_dgitl_summon_doc::SetDest,
            EFFECT_0, TARGET_DEST_NEARBY_ENTRY);
    }
};

/*######
## 69086 Don't Go Into The Light!: Dummy from Character to Geargrinder Fajardo (14239): Gizmo (Fajardo in the spell's
## name) is sorry, he thought the player was dead, and turns back to the Doc (68257 Dummy back to the Doc)
######*/

class spell_lost_isles_dgitl_dummy_to_gizmo : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_dgitl_dummy_to_gizmo);

    // Its nearby-entry target has a range of 0 (self) and no condition names Gizmo: he is looked up within the effect's
    // 13 yards once it is cast
    void HandleAfterCast()
    {
        Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!player)
            return;

        Creature* gizmo = player->FindNearestCreature(NPC_GEARGRINDER_GIZMO, 13.0f);
        if (!gizmo)
            return;

        gizmo->Say(BCT_GIZMO_SORRY_DOC, player);
        if (Creature* doc = FindDocOf(player))
            gizmo->CastSpell(doc, SPELL_DGITL_DUMMY_BACK_TO_DOC, true);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_lost_isles_dgitl_dummy_to_gizmo::HandleAfterCast);
    }
};

/*######
## 69013 Don't Go Into The Light!: Quest Complete (14239): the quest's reward spell, the Doc's last jolt: the player
## comes back to life
######*/

class spell_lost_isles_dgitl_quest_complete : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_dgitl_quest_complete);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetHitPlayer();
        if (!player)
            return;

        player->RemoveAurasDueToSpell(SPELL_NEAR_DEATH);
        player->RemoveAurasDueToSpell(SPELL_COSMETIC_STATE_DEATH);
        player->SetFullHealth();
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_lost_isles_dgitl_quest_complete::HandleScript, EFFECT_0,
            SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

/*######
## 69043 Don't Go Into The Light!: Despawn Doc Zapnozzle (14239): the player's own Doc goes to see to the next survivor
######*/

class spell_lost_isles_dgitl_despawn_doc : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_dgitl_despawn_doc);

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        ObjectGuid const casterGUID = GetCaster()->GetGUID();
        targets.remove_if([casterGUID](WorldObject* target)
        {
            Creature* creature = target->ToCreature();
            return !creature || creature->GetEntry() != NPC_DOC_ZAPNOZZLE || !creature->IsSummon()
                || creature->ToTempSummon()->GetSummonerGUID() != casterGUID;
        });
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        if (Creature* doc = GetHitCreature())
            doc->DespawnOrUnsummon(1s);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_lost_isles_dgitl_despawn_doc::FilterTargets,
            EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
        OnEffectHitTarget += SpellEffectFn(spell_lost_isles_dgitl_despawn_doc::HandleScript, EFFECT_0,
            SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

/*######
## 73702 Blastshadow's Soulstone (Throw It On the Ground!, 25123): using the soulstone (item 52481, from the chest that
## Blastshadow leaves) destroys it
######*/

class spell_lost_isles_blastshadows_soulstone : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_blastshadows_soulstone);

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (Player* player = GetCaster()->ToPlayer())
            player->KilledMonsterCredit(NPC_SOULSTONE_CREDIT);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_lost_isles_blastshadows_soulstone::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

/*######
## 39329 Mine Cart: Wild Mine Cart Ride (25184) from the labor mine down to Assistant Greely at the lumber yard. The
## quest's SourceSpellID 73746 summons the cart and seats the player (73747); the end gives the credit, which also
## starts stage 9.
######*/

Position const MineCartDestination = { 2364.0f, 1928.0f, 22.0f, 0.0f }; // beside Assistant Greely (38124, stage 9)

struct npc_lost_isles_mine_cart : public npc_lost_isles_late_ride
{
    npc_lost_isles_mine_cart(Creature* creature) : npc_lost_isles_late_ride(creature, MineCartDestination, 2.5f, 90s) { }

    // Wild Mine Cart Ride's visual and rattling sounds (88223, 73767, 73768), the client's
    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        DoCastSelf(88223, true);
        DoCastSelf(73767, true);
        DoCastSelf(73768, true);
    }

    void RideEnded(Player* player) override
    {
        player->KilledMonsterCredit(NPC_MINE_CART_RIDE_CREDIT);
    }
};

/*######
## 73947 Escape Velocity: On Interact (Escape Velocity, 25214): a click on a captured goblin straps the rockets to its
## cage and sends it flying
######*/

class spell_lost_isles_escape_velocity : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_escape_velocity);

    SpellCastResult CheckTarget()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player || player->GetQuestStatus(QUEST_ESCAPE_VELOCITY) != QUEST_STATUS_INCOMPLETE)
            return SPELL_FAILED_DONT_REPORT;

        Unit* target = GetExplTargetUnit();
        if (!target || target->GetEntry() != NPC_CAPTURED_GOBLIN || target->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            return SPELL_FAILED_BAD_TARGETS;
        return SPELL_CAST_OK;
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetCaster()->ToPlayer();
        Creature* goblin = GetHitCreature();
        if (!player || !goblin || goblin->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            return;

        player->KilledMonsterCredit(NPC_CAPTURED_GOBLIN);
        goblin->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
        goblin->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
        goblin->CastSpell(goblin, SPELL_ESCAPE_VELOCITY_ROCKETS, true);
        goblin->AI()->Talk(urand(SAY_CAPTURED_GOBLIN_FIRST, SAY_CAPTURED_GOBLIN_LAST), player);
        goblin->SetCanFly(true);
        goblin->SetDisableGravity(true);
        goblin->SetSpeed(MOVE_RUN, 3.0f);
        goblin->SetSpeed(MOVE_FLIGHT, 3.0f);
        Position const sky(goblin->GetPositionX(), goblin->GetPositionY(), goblin->GetPositionZ() + 60.0f);
        goblin->GetMotionMaster()->MovePoint(POINT_CAPTURED_GOBLIN_LAUNCH, sky, FORCED_MOVEMENT_NONE, 0.0f, false);
        goblin->DespawnOrUnsummon(4s);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_lost_isles_escape_velocity::CheckTarget);
        OnEffectHitTarget += SpellEffectFn(spell_lost_isles_escape_velocity::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

/*######
## 39598 Ultimate Footbomb Uniform: the vehicle of Final Confrontation (25251), summoned by the spell click on 39592
## (73991; riding it, 73989, gives the uniform's credit). It goes when the player gets out.
######*/

struct npc_lost_isles_footbomb_uniform : public VehicleAI
{
    npc_lost_isles_footbomb_uniform(Creature* creature) : VehicleAI(creature) { }

    void PassengerBoarded(Unit* passenger, int8 /*seatId*/, bool apply) override
    {
        if (passenger->IsPlayer() && !apply)
            me->DespawnOrUnsummon(1s);
    }
};

/*######
## 39582 Trade Prince Gallywix (the docks): Final Confrontation (25251). He fights only when attacked, with the client's
## spells for him (Downsizing, Unload Toxic Assets, Revenue Stream) and TDB 434's lines; at 5% health he gives up
## ("Uncle!"), the players nearby on the quest leave their uniforms and get his credit (which starts stage 10), and he
## resets. He cannot die.
######*/

enum FinalConfrontation
{
    EVENT_GALLYWIX_DOWNSIZING       = 1,
    EVENT_GALLYWIX_TOXIC_ASSETS,
    EVENT_GALLYWIX_REVENUE_STREAM,
    EVENT_GALLYWIX_GIVES_UP,
    EVENT_GALLYWIX_RESET,

    SAY_GALLYWIX_MOOLAH             = 2,
    SAY_GALLYWIX_TRAITOR            = 3,
    SAY_GALLYWIX_TOXIC_SUCKER       = 4,
    SAY_GALLYWIX_TRIM_THE_FAT       = 5,
    SAY_GALLYWIX_UNLOAD_TOXIC       = 6,
    SAY_GALLYWIX_DOWNSIZING_CARTEL  = 7,
    SAY_GALLYWIX_UNCLE              = 8,
};

struct npc_lost_isles_gallywix_docks : public ScriptedAI
{
    npc_lost_isles_gallywix_docks(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        // Also after giving up: back to full health and ready for the next player
        _events.Reset();
        _defeated = false;
        me->SetFullHealth();
        me->SetImmuneToAll(false);
        me->SetReactState(REACT_DEFENSIVE);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        Talk(SAY_GALLYWIX_TRAITOR);
        _events.ScheduleEvent(EVENT_GALLYWIX_DOWNSIZING, 8s);
        _events.ScheduleEvent(EVENT_GALLYWIX_REVENUE_STREAM, 12s);
        _events.ScheduleEvent(EVENT_GALLYWIX_TOXIC_ASSETS, 18s);
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damagetype*/,
        SpellSchoolMask /*damageSchoolMask*/) override
    {
        if (!_defeated && !me->HealthBelowPctDamaged(5, damage))
            return;

        damage = 0;
        if (_defeated)
            return;

        // The credits wait for the next update: the damage comes from a uniform the player is about to leave
        _defeated = true;
        _events.Reset();
        _events.ScheduleEvent(EVENT_GALLYWIX_GIVES_UP, 1ms);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_GALLYWIX_DOWNSIZING:
                    Talk(urand(0, 1) ? SAY_GALLYWIX_TRIM_THE_FAT : SAY_GALLYWIX_DOWNSIZING_CARTEL);
                    DoCastVictim(SPELL_DOWNSIZING);
                    _events.ScheduleEvent(EVENT_GALLYWIX_DOWNSIZING, 15s, 20s);
                    break;
                case EVENT_GALLYWIX_REVENUE_STREAM:
                    Talk(SAY_GALLYWIX_MOOLAH);
                    DoCastVictim(SPELL_REVENUE_STREAM);
                    _events.ScheduleEvent(EVENT_GALLYWIX_REVENUE_STREAM, 14s, 18s);
                    break;
                case EVENT_GALLYWIX_TOXIC_ASSETS:
                    Talk(urand(0, 1) ? SAY_GALLYWIX_TOXIC_SUCKER : SAY_GALLYWIX_UNLOAD_TOXIC);
                    DoCastVictim(SPELL_UNLOAD_TOXIC_ASSETS);
                    _events.ScheduleEvent(EVENT_GALLYWIX_TOXIC_ASSETS, 20s, 25s);
                    break;
                case EVENT_GALLYWIX_GIVES_UP:
                    Talk(SAY_GALLYWIX_UNCLE);
                    me->AttackStop();
                    me->SetReactState(REACT_PASSIVE);
                    me->SetImmuneToAll(true);
                    // Out of the uniform first: the credit moves the player into stage 10, where it does not exist
                    for (Player* player : GetPlayersInRange(me, 60.0f))
                        if (player->GetQuestStatus(QUEST_FINAL_CONFRONTATION) == QUEST_STATUS_INCOMPLETE)
                        {
                            if (player->GetVehicle())
                                player->ExitVehicle();
                            player->KilledMonsterCredit(NPC_TRADE_PRINCE_GALLYWIX_DOCKS);
                        }
                    _events.ScheduleEvent(EVENT_GALLYWIX_RESET, 5s);
                    break;
                case EVENT_GALLYWIX_RESET:
                    EnterEvadeMode();
                    break;
                default:
                    break;
            }
        }

        if (_defeated || !UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
    bool _defeated = false;
};

} // namespace LostIsles

void AddSC_zone_lost_isles()
{
    using namespace LostIsles;
    RegisterCreatureAI(npc_lost_isles_doc_zapnozzle);
    RegisterCreatureAI(npc_lost_isles_gyrochoppa_pilot);
    RegisterCreatureAI(npc_lost_isles_geargrinder_gizmo);
    RegisterGameObjectAI(go_lost_isles_escape_pod);
    RegisterCreatureAI(npc_lost_isles_pod_survivor);
    RegisterCreatureAI(npc_lost_isles_sassy_hardwrench_beach);
    RegisterCreatureAI(npc_lost_isles_quest_talker);
    RegisterCreatureAI(npc_lost_isles_gallywix_beach);
    RegisterCreatureAI(npc_lost_isles_bomb_throwing_monkey);
    RegisterCreatureAI(npc_lost_isles_foreman_dampwick_mine);
    RegisterCreatureAI(npc_lost_isles_frightened_miner);
    new player_lost_isles_miner_troubles();
    RegisterSpellScript(spell_lost_isles_miner_quest_accept);
    RegisterSpellScript(spell_lost_isles_frightened_miner_controller);
    RegisterSpellScript(spell_lost_isles_despawn_frightened_miner);
    RegisterSpellScript(spell_lost_isles_new_miner_from_gossip);
    RegisterCreatureAI(npc_lost_isles_kajamite_chunk);
    RegisterSpellScript(spell_lost_isles_chunk_ride_ore_cart);
    RegisterCreatureAI(npc_lost_isles_capturing_bunny);
    RegisterSpellScript(spell_lost_isles_ktc_snapflash);
    RegisterSpellScript(spell_lost_isles_capturing_aicast);
    RegisterSpellScript(spell_lost_isles_capturing_sight);
    RegisterSpellScript(spell_lost_isles_capturing_remove_sight);
    new player_lost_isles_capturing_the_unknown();
    RegisterCreatureAI(npc_lost_isles_pygmy_witchdoctor);
    RegisterCreatureAI(npc_lost_isles_vale_orc);
    RegisterCreatureAI(npc_lost_isles_vale_plant);
    RegisterCreatureAI(npc_lost_isles_strangle_vine);
    RegisterCreatureAI(npc_lost_isles_orc_scout_leaving);
    RegisterSpellScript(spell_lost_isles_weed_whacker);
    RegisterSpellScript(spell_lost_isles_weed_whacker_aura);
    RegisterSpellScript(spell_lost_isles_weed_whacker_despawn_bunny);
    RegisterCreatureAI(npc_lost_isles_weed_whacker_bunny);
    RegisterSpellScript(spell_lost_isles_despawn_orc_scout);
    RegisterSpellScript(spell_lost_isles_new_orc_scout);
    new player_lost_isles_infrared();
    RegisterCreatureAI(npc_lost_isles_si7_assassin);
    new item_lost_isles_weed_whacker();
    new item_lost_isles_infrared_heat_focals();
    RegisterCreatureAI(npc_lost_isles_kilag_gorefang_overlook);
    RegisterCreatureAI(npc_lost_isles_orc_scout_infrared);
    RegisterCreatureAI(npc_lost_isles_bastia);
    RegisterCreatureAI(npc_lost_isles_gyrochoppa_quest);
    RegisterCreatureAI(npc_lost_isles_gyrochoppa);
    RegisterCreatureAI(npc_lost_isles_thrall_caged);
    RegisterCreatureAI(npc_lost_isles_thrall_freed);
    RegisterCreatureAI(npc_lost_isles_thrall_up_top);
    RegisterCreatureAI(npc_lost_isles_ship_fire_stalker);
    RegisterCreatureAI(npc_lost_isles_alliance_sailor);
    RegisterCreatureAI(npc_lost_isles_cyclone_of_the_elements);
    RegisterCreatureAI(npc_lost_isles_elm_bunny);
    RegisterCreatureAI(npc_lost_isles_warrior_matic_camp);
    RegisterCreatureAI(npc_lost_isles_sassy_hardwrench_camp);
    RegisterCreatureAI(npc_lost_isles_orc_survivor);
    RegisterCreatureAI(npc_lost_isles_gallywix_camp);
    RegisterCreatureAI(npc_lost_isles_sling_rocket_gallywix);
    RegisterGameObjectAI(go_lost_isles_rocket_sling);
    RegisterCreatureAI(npc_lost_isles_sling_rocket);
    RegisterCreatureAI(npc_lost_isles_town_defender);
    RegisterCreatureAI(npc_lost_isles_town_quest_giver);
    RegisterCreatureAI(npc_lost_isles_hobart_grapplehammer);
    RegisterCreatureAI(npc_lost_isles_megs_dreadshredder);
    RegisterCreatureAI(npc_lost_isles_mechashark);
    RegisterCreatureAI(npc_lost_isles_bc_eliminator);
    RegisterSpellScript(spell_lost_isles_old_friends_sassy_yell);
    RegisterCreatureAI(npc_lost_isles_pride_of_kezan);
    RegisterCreatureAI(npc_lost_isles_stealth_fighter);
    RegisterSpellScript(spell_lost_isles_sweet_oil_explosion_check);
    RegisterCreatureAI(npc_lost_isles_sweet_oil_controller);
    RegisterCreatureAI(npc_lost_isles_volcano_boulder_bunny);
    RegisterCreatureAI(npc_lost_isles_pterrordax_scavenger);
    RegisterSpellScript(spell_lost_isles_rocket_boots_item);
    RegisterCreatureAI(npc_lost_isles_rocket_boots);
    RegisterCreatureAI(npc_lost_isles_blastshadow);
    RegisterSpellScript(spell_lost_isles_town_in_a_box_plunger);
    RegisterSpellScript(spell_lost_isles_rope_ladder);
    RegisterCreatureAI(npc_lost_isles_wild_clucker);
    RegisterCreatureAI(npc_lost_isles_wild_clucker_egg);
    RegisterCreatureAI(npc_lost_isles_spiny_raptor);
    RegisterCreatureAI(npc_lost_isles_mechachicken);
    RegisterCreatureAI(npc_lost_isles_biggest_egg_visual);
    RegisterCreatureAI(npc_lost_isles_vashjelan_warrior);
    RegisterCreatureAI(npc_lost_isles_vashjelan_siren);
    RegisterCreatureAI(npc_lost_isles_naga_hatchling);
    RegisterCreatureAI(npc_lost_isles_naga_hatchling_follower);
    RegisterCreatureAI(npc_lost_isles_naga_hatchling_hostage);
    RegisterCreatureAI(npc_lost_isles_ace);
    RegisterCreatureAI(npc_lost_isles_faceless_of_the_deep);
    RegisterCreatureAI(npc_lost_isles_oomlot_warrior);
    RegisterCreatureAI(npc_lost_isles_yngwie);
    RegisterCreatureAI(npc_lost_isles_goblin_captive);
    RegisterCreatureAI(npc_lost_isles_oomlot_shaman);
    RegisterCreatureAI(npc_lost_isles_pygmy_leader);
    RegisterCreatureAI(npc_lost_isles_pygmy_captive);
    RegisterCreatureAI(npc_lost_isles_goblin_zombie);
    RegisterSpellScript(spell_lost_isles_zombie_targets);
    RegisterCreatureAI(npc_lost_isles_zombie_shooter);
    RegisterCreatureAI(npc_lost_isles_volcanoth_champion);
    RegisterCreatureAI(npc_lost_isles_doc_zapnozzle_field);
    RegisterCreatureAI(npc_lost_isles_volcanoth);
    new player_lost_isles_volcanoth();
    new player_lost_isles_dont_go_into_the_light();
    new player_lost_isles_escape_pods();
    RegisterCreatureAI(npc_lost_isles_volcanoth_explosion_bunny);
    RegisterCreatureAI(npc_lost_isles_sassy_hardwrench_volcano);
    RegisterCreatureAI(npc_lost_isles_flying_bomber);
    RegisterCreatureAI(npc_lost_isles_giant_fireball_bunny);
    RegisterCreatureAI(npc_lost_isles_orc_battlesworn);
    RegisterCreatureAI(npc_lost_isles_bastia_mine);
    RegisterSpellScript(spell_lost_isles_kaja_cola_zero_one);
    RegisterSpellScript(spell_lost_isles_blastshadows_soulstone);
    RegisterCreatureAI(npc_lost_isles_mine_cart);
    RegisterSpellScript(spell_lost_isles_escape_velocity);
    RegisterSpellScript(spell_lost_isles_dgitl_placement);
    RegisterSpellScript(spell_lost_isles_dgitl_quest_complete);
    RegisterSpellScript(spell_lost_isles_dgitl_summon_doc);
    RegisterSpellScript(spell_lost_isles_dgitl_dummy_to_gizmo);
    RegisterSpellScript(spell_lost_isles_dgitl_despawn_doc);
    RegisterCreatureAI(npc_lost_isles_footbomb_uniform);
    RegisterCreatureAI(npc_lost_isles_gallywix_docks);
}
