/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
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

// This is where scripts' loading functions should be declared:
void AddSC_account_commandscript();
void AddSC_achievement_commandscript();
void AddSC_ahbot_commandscript();
void AddSC_arena_commandscript();
void AddSC_ban_commandscript();
void AddSC_bf_commandscript();
void AddSC_bg_commandscript();
void AddSC_cast_commandscript();
void AddSC_character_commandscript();
void AddSC_cheat_commandscript();
void AddSC_debug_commandscript();
void AddSC_deserter_commandscript();
void AddSC_disable_commandscript();
void AddSC_event_commandscript();
void AddSC_gm_commandscript();
void AddSC_go_commandscript();
void AddSC_dummy_commandscript();
void AddSC_worldtools_commandscript();
void AddSC_gobject_commandscript();
void AddSC_npcstate_commandscript();
void AddSC_scene_commandscript();
void AddSC_group_commandscript();
void AddSC_guild_commandscript();
void AddSC_statpersist_hook();
void AddSC_appearance_commandscript();
void AddSC_botdiag_commandscript();
void AddSC_botfind_commandscript();
void AddSC_modelfind_commandscript();
void AddSC_wp_commandscript();
void AddSC_dragonriding_commandscript();
void AddSC_botrename_commandscript();
void AddSC_botpersist_commandscript();     // step49 游荡bot永久化
void AddSC_pbot_autoaccept();
void AddSC_honor_commandscript();
void AddSC_instance_commandscript();
void AddSC_playerbot_commandscript();
void AddSC_learn_commandscript();
void AddSC_lfg_commandscript();
void AddSC_emote_commandscript();
void AddSC_say_commandscript();
void AddSC_list_commandscript();
void AddSC_lookup_commandscript();
void AddSC_combathelper_commandscript();
void AddSC_message_commandscript();
void AddSC_misc_commandscript();
void AddSC_mmaps_commandscript();
void AddSC_modify_commandscript();
void AddSC_itemforge_commandscript();
void AddSC_transmog_commandscript();
void AddSC_spellclean_commandscript();
void AddSC_reloaditem_commandscript();
void AddSC_gmhelper_commandscript();
void AddSC_gearset_commandscript();
void AddSC_smartadd_commandscript();
void AddSC_npc_commandscript();
void AddSC_pet_commandscript();
void AddSC_quest_commandscript();
void AddSC_rbac_commandscript();
void AddSC_reload_commandscript();
void AddSC_reset_commandscript();
void AddSC_send_commandscript();
void AddSC_server_commandscript();
void AddSC_tele_commandscript();
void AddSC_ticket_commandscript();
void AddSC_titles_commandscript();

// The name of this function should match:
// void Add${NameOfDirectory}Scripts()
void AddCommandsScripts()
{
    AddSC_account_commandscript();
    AddSC_achievement_commandscript();
    AddSC_ahbot_commandscript();
    AddSC_arena_commandscript();
    AddSC_ban_commandscript();
    AddSC_bf_commandscript();
    AddSC_bg_commandscript();
    AddSC_cast_commandscript();
    AddSC_character_commandscript();
    AddSC_cheat_commandscript();
    AddSC_debug_commandscript();
    AddSC_playerbot_commandscript();
    AddSC_wp_commandscript();
    AddSC_dragonriding_commandscript();
    AddSC_botrename_commandscript();
    AddSC_botpersist_commandscript();      // step49 游荡bot永久化
    AddSC_pbot_autoaccept();           // step42 PlayerBot自动接受
    AddSC_deserter_commandscript();
    AddSC_botfind_commandscript();
    AddSC_modelfind_commandscript();
    AddSC_botdiag_commandscript();
    AddSC_disable_commandscript();
    AddSC_say_commandscript();
    AddSC_appearance_commandscript();
    AddSC_worldtools_commandscript();
    AddSC_event_commandscript();
    AddSC_gm_commandscript();
    AddSC_go_commandscript();
    AddSC_emote_commandscript();
    AddSC_gobject_commandscript();
    AddSC_group_commandscript();
    AddSC_npcstate_commandscript();
    AddSC_scene_commandscript();
    AddSC_guild_commandscript();
    AddSC_statpersist_hook();
    AddSC_dummy_commandscript();
    AddSC_honor_commandscript();
    AddSC_combathelper_commandscript();
    AddSC_instance_commandscript();
    AddSC_learn_commandscript();
    AddSC_itemforge_commandscript();
    AddSC_lookup_commandscript();
    AddSC_transmog_commandscript();
    AddSC_spellclean_commandscript();
    AddSC_reloaditem_commandscript();
    AddSC_gmhelper_commandscript();
    AddSC_lfg_commandscript();
    AddSC_list_commandscript();
    AddSC_message_commandscript();
    AddSC_misc_commandscript();
    AddSC_mmaps_commandscript();
    AddSC_modify_commandscript();
    AddSC_gearset_commandscript();
    AddSC_smartadd_commandscript();
    AddSC_npc_commandscript();
    AddSC_quest_commandscript();
    AddSC_pet_commandscript();
    AddSC_rbac_commandscript();
    AddSC_reload_commandscript();
    AddSC_reset_commandscript();
    AddSC_send_commandscript();
    AddSC_server_commandscript();
    AddSC_tele_commandscript();
    AddSC_ticket_commandscript();
    AddSC_titles_commandscript();
}
