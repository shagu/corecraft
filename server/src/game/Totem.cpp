/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Totem.h"
#include "CreatureAI.h"
#include "DBCStores.h"
#include "Group.h"
#include "InstanceData.h"
#include "logging.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SpellMgr.h"
#include "WorldPacket.h"

Totem::Totem() : Creature(CREATURE_SUBTYPE_TOTEM)
{
    m_duration = 0;
    m_type = TOTEM_PASSIVE;
}

bool Totem::Create(uint32 guidlow, CreatureCreatePos& cPos,
    CreatureInfo const* cinfo, Unit* owner)
{
    SetMap(cPos.GetMap());

    Team team = owner->GetTypeId() == TYPEID_PLAYER ?
                    ((Player*)owner)->GetTeam() :
                    TEAM_NONE;

    if (!CreateFromProto(guidlow, cinfo, team))
        return false;

    // special model selection case for totems
    if (owner->GetTypeId() == TYPEID_PLAYER)
    {
        if (uint32 modelid_race = sObjectMgr::Instance()->GetModelForRace(
                GetNativeDisplayId(), owner->getRaceMask()))
            SetDisplayId(modelid_race);
    }

    cPos.SelectFinalPoint(this);

    // totem must be at same Z in case swimming caster and etc.
    if (fabs(cPos.m_pos.z - owner->GetZ()) > 5.0f)
        cPos.m_pos.z = owner->GetZ();

    if (!cPos.Relocate(this))
        return false;

    // Notify the map's instance data.
    // Only works if you create the object in it, not if it is moves to that
    // map.
    // Normally non-players do not teleport to other maps.
    if (InstanceData* iData = GetMap()->GetInstanceData())
        iData->OnCreatureCreate(this);

    LoadCreatureAddon(false);

    return true;
}

void Totem::Update(uint32 update_diff, uint32 time)
{
    if (has_queued_actions())
        update_queued_actions(update_diff);

    // Totems can be owner-less
    Unit* owner = GetOwner();
    if (GetOwnerGuid() && (!owner || !owner->isAlive() || !isAlive()))
    {
        UnSummon(); // remove self
        return;
    }

    if (m_duration <= update_diff)
    {
        UnSummon(); // remove self
        return;
    }
    else
        m_duration -= update_diff;

    Creature::Update(update_diff, time);
}

bool Totem::Summon(Unit* owner)
{
    if (!owner->GetMap()->insert(this))
        return false;

    auto guid = owner->GetObjectGuid();

    // Do summon actions after we've been fully added to map
    queue_action(0, [this, guid]()
        {
            auto owner = GetMap()->GetUnit(guid);
            if (!owner)
                return;

            AIM_Initialize();

            WorldPacket data(SMSG_GAMEOBJECT_SPAWN_ANIM_OBSOLETE, 8);
            data << GetObjectGuid();
            SendMessageToSet(&data, true);

            if (owner->GetTypeId() == TYPEID_UNIT && ((Creature*)owner)->AI())
                ((Creature*)owner)->AI()->JustSummoned((Creature*)this);
            if (AI())
                AI()->SummonedBy(owner);

            // there are some totems, which exist just for their visual
            // appeareance
            if (!GetSpell())
                return;

            switch (m_type)
            {
            case TOTEM_PASSIVE:
                CastSpell(this, GetSpell(), true);
                break;
            case TOTEM_STATUE:
                CastSpell(GetOwner(), GetSpell(), true);
                break;
            default:
                break;
            }

        });

    return true;
}

void Totem::UnSummon()
{
    SendObjectDeSpawnAnim(GetObjectGuid());

    CombatStop();
    InterruptNonMeleeSpells(false);
    remove_auras();

    if (Unit* owner = GetOwner())
    {
        owner->_RemoveTotem(this);
        owner->remove_auras_if([this](AuraHolder* holder)
            {
                return holder->GetId() == GetSpell() &&
                       holder->GetCasterGuid() == GetObjectGuid();
            });

        // remove aura all party members too
        if (owner->GetTypeId() == TYPEID_PLAYER)
        {
            // Not only the player can summon the totem (scripted AI)
            if (Group* group = ((Player*)owner)->GetGroup())
            {
                for (auto member : group->members(true))
                {
                    if (group->SameSubGroup((Player*)owner, member))
                        member->remove_auras_if([this](AuraHolder* holder)
                            {
                                return holder->GetId() == GetSpell() &&
                                       holder->GetCasterGuid() ==
                                           GetObjectGuid();
                            });
                }
            }
        }

        if (owner->GetTypeId() == TYPEID_UNIT && ((Creature*)owner)->AI())
            ((Creature*)owner)->AI()->SummonedCreatureDespawn((Creature*)this);
    }

    // any totem unsummon look like as totem kill, req. for proper animation
    if (isAlive())
        SetDeathState(DEAD);

    // Totem might get removed from world by removing the owners aura
    if (IsInWorld())
        AddObjectToRemoveList();
}

void Totem::SetOwner(Unit* owner)
{
    SetCreatorGuid(owner->GetObjectGuid());
    SetOwnerGuid(owner->GetObjectGuid());
    setFaction(owner->getFaction());
    SetLevel(owner->getLevel());
}

Unit* Totem::GetOwner()
{
    if (ObjectGuid ownerGuid = GetOwnerGuid())
        return ObjectAccessor::GetUnit(*this, ownerGuid);

    return nullptr;
}

void Totem::SetTypeBySummonSpell(SpellEntry const* spellProto)
{
    // Get spell casted by totem
    SpellEntry const* totemSpell = sSpellStore.LookupEntry(GetSpell());
    if (totemSpell)
    {
        // If spell have cast time -> so its active totem
        if (GetSpellCastTime(totemSpell) ||
            totemSpell->Mechanic == MECHANIC_CHARM)
            m_type = TOTEM_ACTIVE;
    }
    if (spellProto->SpellIconID == 2056)
        m_type = TOTEM_STATUE; // Jewelery statue
}

bool Totem::IsImmuneToSpellEffect(
    SpellEntry const* spellInfo, SpellEffectIndex index) const
{
    // Check for Mana Spring & Healing Stream totems
    switch (spellInfo->SpellFamilyName)
    {
    case SPELLFAMILY_SHAMAN:
        if (spellInfo->IsFitToFamilyMask(UI64LIT(0x00000002000)) ||
            spellInfo->IsFitToFamilyMask(UI64LIT(0x00000004000)))
            return false;
        break;
    default:
        break;
    }

    switch (spellInfo->Effect[index])
    {
    case SPELL_EFFECT_ATTACK_ME:
    // immune to any type of regeneration effects hp/mana etc.
    case SPELL_EFFECT_HEAL:
    case SPELL_EFFECT_HEAL_MAX_HEALTH:
    case SPELL_EFFECT_HEAL_MECHANICAL:
    case SPELL_EFFECT_HEAL_PCT:
    case SPELL_EFFECT_ENERGIZE:
    case SPELL_EFFECT_ENERGIZE_PCT:
        return true;
    default:
        break;
    }

    if (!IsPositiveSpell(spellInfo))
    {
        // immune to all negative auras
        if (IsAuraApplyEffect(spellInfo, index))
            return true;
    }
    else
    {
        // immune to any type of regeneration auras hp/mana etc.
        if (IsPeriodicRegenerateEffect(spellInfo, index))
            return true;
    }

    return Creature::IsImmuneToSpellEffect(spellInfo, index);
}
