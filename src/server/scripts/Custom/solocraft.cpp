﻿/*
 * This file is part of the DestinyCore Project. See AUTHORS file for Copyright information
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

#include <map>
#include "Config.h"
#include "ScriptMgr.h"
#include "Unit.h"
#include "Player.h"
#include "Pet.h"
#include "Map.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Chat.h"
#include "Log.h"
#include "SpellScript.h"
#include "ChallengeMgr.h"
#include "ScenarioMgr.h"
#include "GameTables.h"
#include "SpellAuraEffects.h"

 /*
  * TODO:
  * 1. Dispel target regeneration
  * 2. Provide unlimited http://www.wowhead.com/item=17333/aqual-quintessence
  */

namespace {

	class solocraft_player_instance_handler : public PlayerScript {

	public:
		solocraft_player_instance_handler() : PlayerScript("solocraft_player_instance_handler") {
			TC_LOG_INFO("entities.player.character", "[Solocraft] solocraft_player_instance_handler Loaded");
		}

		void OnMapChanged(Player* player) override {
			Map* map = player->GetMap();
			float  difficulty = CalculateDifficulty(map, player);
			int numInGroup = GetNumInGroup(player);
			ApplyBuffs(player, map, difficulty, numInGroup);
		}
	private:
		std::map<ObjectGuid, float> _unitDifficulty;

		float  CalculateDifficulty(Map* map, Player* player) {
			float difficulty = 1.0;
			if (map) {
				if (map->Is25ManRaid()) {
					difficulty = 35;
				}
				else if (map->IsHeroic()) {
					difficulty = 20;
				}
				else if (map->IsRaid()) {
					difficulty = 55;
				}
				else if (map->IsDungeon()) {
					difficulty = 10;
				}
			}
			return difficulty - 1.0; // HandleStatModifier treats 100 as +100%, so a difficulty of 1 ends up being applied as a 200% buff
		}

		int GetNumInGroup(Player* player) {
			int numInGroup = 1;
			Group* group = player->GetGroup();
			if (group) {
				Group::MemberSlotList const& groupMembers = group->GetMemberSlots();
				numInGroup = groupMembers.size();
			}
			return numInGroup;
		}

		void ApplyBuffs(Player* player, Map* map, float  difficulty, int numInGroup) {
			ClearBuffs(player, map);
			if (difficulty != 0) {
				player->CastSpell(player, SPELL_MINERVA_BUFF, true);

				//InstanceMap *instanceMap = map->ToInstanceMap();
				//InstanceScript *instanceScript = instanceMap->GetInstanceScript();

				// Announce to player
				std::ostringstream ss;
				ss << "|cffFF0000[SoloCraft] |cffFF8000" << player->GetName() << " entered %s - # of Players: %d - Difficulty Offset: %0.2f.";
				ChatHandler(player->GetSession()).PSendSysMessage(ss.str().c_str(), map->GetMapName(), numInGroup, difficulty + 1.0);

				_unitDifficulty[player->GetGUID()] = difficulty;
				for (int32 i = STAT_STRENGTH; i < MAX_STATS; ++i)
				{
					player->HandleStatModifier(UnitMods(UNIT_MOD_STAT_START + i), TOTAL_PCT, float(difficulty * 100), true);
				}

				bool m_Solocraft = sConfigMgr->GetBoolDefault("Solocraft.Enable", false);
				if (m_Solocraft)
				{

					player->HandleStatModifier(UnitMods(UNIT_MOD_MANA), TOTAL_PCT, (difficulty * 80), true);// Total mana
					player->HandleStatModifier(UnitMods(UNIT_MOD_HEALTH), TOTAL_PCT, (difficulty * 80), true);
					//player->HandleStatModifier(UnitMods(UNIT_MOD_STAT_START), TOTAL_PCT, (difficulty * 100), true);

					player->RemoveAurasDueToSpell(300047);// pour risque de doublon , retire toutes les auras 300047
					player->AddAura(300047, player);
					//player->AddAura(110055, player);
					//player->AddAura(160137, player);

					player->SetFullHealth();
					if (player->getPowerType() == POWER_MANA) {
						player->SetPower(POWER_MANA, player->GetMaxPower(POWER_MANA));

					}
				}
			}
		}


		void ClearBuffs(Player* player, Map* map) {

			player->RemoveAurasDueToSpell(SPELL_MINERVA_BUFF);

			std::map<ObjectGuid, float>::iterator unitDifficultyIterator = _unitDifficulty.find(player->GetGUID());
			if (unitDifficultyIterator != _unitDifficulty.end()) {
				float difficulty = unitDifficultyIterator->second;
				_unitDifficulty.erase(unitDifficultyIterator);

				// Inform the player
				std::ostringstream ss;
				ss << "|cffFF0000[SoloCraft] |cffFF8000" << player->GetName() << " exited to %s - Reverting Difficulty Offset: %0.2f.";
				ChatHandler(player->GetSession()).PSendSysMessage(ss.str().c_str(), map->GetMapName(), difficulty + 1.0);

				for (int32 i = STAT_STRENGTH; i < MAX_STATS; ++i) {
					player->HandleStatModifier(UnitMods(UNIT_MOD_STAT_START + i), TOTAL_PCT, (difficulty * 100, 0), false);
				}

				bool m_Solocraft = sConfigMgr->GetBoolDefault("Solocraft.Enable", false);
				if (m_Solocraft)
				{
					player->HandleStatModifier(UnitMods(UNIT_MOD_MANA), TOTAL_PCT, (difficulty * 80), false);
					player->HandleStatModifier(UnitMods(UNIT_MOD_HEALTH), TOTAL_PCT, (difficulty * 80), false);
					//player->HandleStatModifier(UnitMods(UNIT_MOD_STAT_START), TOTAL_PCT, (difficulty * 100), false);
					player->RemoveAurasDueToSpell(300047);
					//player->RemoveAurasDueToSpell(110055);
					//player->RemoveAurasDueToSpell(160137);// pour risque de doublon , retire toutes les auras 300047
				}

			}
		}
	};

	// 305306 - Minerva's buff
	class spell_minerva_buff : public AuraScript
	{
		PrepareAuraScript(spell_minerva_buff);

		enum BuffScaling
		{
			CHALLENGE_LEVEL_MIN = 2,
			CHALLENGE_LEVEL_MAX = 30,
		};

		void CalculateAmount(AuraEffect const* aurEff, float& amount, bool& /*canBeRecalculated*/)
		{
			auto caster = GetCaster();

			Scenario* progress = sScenarioMgr->GetScenario(caster->GetInstanceId());
			if (!progress)
				return;

			auto const& challenge = progress->GetChallenge();
			if (!challenge)
				return;

			uint32 challengeLevel = challenge->GetChallengeLevel();
			GtChallengeModeHealthEntry const* gtHealth = sChallengeModeHealthTable.GetRow(challengeLevel);
			GtChallengeModeDamageEntry const* gtDamage = sChallengeModeDamageTable.GetRow(challengeLevel);
			if (!gtHealth || !gtDamage || challengeLevel < CHALLENGE_LEVEL_MIN)
				return;

			switch (aurEff->GetEffIndex())
			{
			case EFFECT_0:
				amount = aurEff->GetBaseAmount() * gtHealth->Scalar;
				break;
			case EFFECT_1:
				amount = scale(CHALLENGE_LEVEL_MIN, CHALLENGE_LEVEL_MAX, aurEff->GetBaseAmount(), aurEff->GetMiscValueB(), challengeLevel);
				break;
			default:
				break;
			}
		}

		float scale(float oldMin, float oldMax, float newMin, float newMax, float oldValue)
		{
			if (oldValue > oldMax)
				return newMax;
			else if (oldValue < oldMin)
				return newMin;

			float oldRange = (oldMax - oldMin);
			float newRange = (newMax - newMin);
			float newValue = (((oldValue - oldMin) * newRange) / oldRange) + newMin;

			return newValue;
		}

		void Register() override
		{
			DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_minerva_buff::CalculateAmount, EFFECT_0, SPELL_AURA_MOD_HEALING_PCT);
			DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_minerva_buff::CalculateAmount, EFFECT_1, SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
		}
	};
}

void AddSC_solocraft() {
	new solocraft_player_instance_handler();
	RegisterAuraScript(spell_minerva_buff);
}