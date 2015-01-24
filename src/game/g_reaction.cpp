/**
 * @file
 * @brief Reaction fire code
 *
 * Note that in a turn-based game, reaction fire is a design bug in the first place,
 * causing several logic problems. But we want it and we need it, so we'll have to
 * work around those problems.
 *
 * Imagine the following situations:
 * One of your soldiers is standing next to a house by a street. There is an alien down
 * the street with a gun in snap shot mode (8 TUs). The soldier steps out on street, trying
 * to cross it (and entering the line of sight of that alien). After 4 more paces or spending
 * 8 TUs, your soldier is shot by the alien. Sounds reasonable? Fine. That's the basic idea
 * of reaction fire.
 *
 * Now assume you have 5 soldiers next to that house. They all step out on the street. Nothing
 * happens because they all just entered the line of sight of the alien. The first soldier
 * starts walking and gets shot after 4 paces like above. The second soldier will walk 7 paces
 * unharmed and get shot after 8 paces and the third soldier will be shot after 12 paces/24 TUs.
 * This is because when the alien shoots at one target, all his other targets will receive a
 * bonus equal to the amount of TUs the alien used for his shot. Still sounds reasonable? Fine.
 *
 * A slight modification: only one of the 5 soldiers steps out and gets shot after 4 paces.
 * Then the 2nd steps out and gets shot after 4 paces as well. Likewise the 3rd soldier.
 * That's because the soldiers hidden behind the house are not among the targets of the alien
 * and thus don't receive the bonus when the alien shoots,
 *
 * There is also a problem at end of turn. Imagine your sniper is set to react with an aimed
 * shot (18 Tus). An alien steps into his line of sight and fires two snap shots, totaling
 * 16 TUs. Then the alien decides to do nothing for the rest of his round. You would love to
 * see your sniper do his aimed shot, right ? But reaction fire rules don't allow that.
 * On the other hand: if you (were stupid enough to) move one of your soldiers with his last
 * 2 TUs out from cover and into the sight of an alien on RF with all his TUs still available,
 * your soldier will receive no RF, due to the same RF rules. You love that, right ?
 *
 * Reaction fire is involved in the following situations:
 * 1. G_ReactionFireOnMovement()
 *		calls	G_ReactionFireCheckExecution()
 *				calls	G_ReactionFireTryToShoot()
 *		calls	G_ReactionFireTargetsUpdateAll()
 * 2. G_ClientShoot()
 *		calls	G_ReactionFirePreShot()
 *				calls	G_ReactionFireTargetsUpdateAll()
 *				calls	G_ReactionFireTryToShoot()
 *		calls	G_ReactionFirePostShot()
 *				calls	G_ReactionFireCheckExecution()
 *						calls	G_ReactionFireTryToShoot()
 * 3. G_ClientEndRound()
 *		calls	G_ReactionFireOnEndTurn()
 *				calls	G_ReactionFireTryToShoot()
 *		calls	G_ReactionFireReset()
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "g_reaction.h"
#include "g_actor.h"
#include "g_client.h"
#include "g_combat.h"
#include "g_edicts.h"
#include "g_match.h"
#include "g_vis.h"

#define MAX_RF_TARGETS 10
#define MAX_RF_DATA 128

#define DEBUG_RF 0

/** @brief A single relation between a shooter and his target. */
class ReactionFireTarget
{
public:
	Edict const* target;
	int triggerTUs;		/* the amount of TUS of the target(!) at which the reaction takes place */
};

#define RF_NO_ENTNUM -1

/** @brief A list of relations between a shooter and all his targets. */
class ReactionFireTargetList
{
public:
	int entnum;
	int count;
	ReactionFireTarget targets[MAX_RF_TARGETS];

	ReactionFireTargetList () {
		OBJZERO(targets);
		init();
	}

	inline void init (void) {
		entnum = RF_NO_ENTNUM;
		count = 0;
	}

	inline void reset (void) {
		count = 0;
	}
};

/** @brief A table with all the relations between all shooters and all their targets. */
class ReactionFireTargets
{
public:
	void init();
	void add(const Edict* shooter, const Edict* target, const int tusForShot);
	void remove(const Edict* shooter, const Edict* target);
	bool hasExpired(const Edict* shooter, const Edict* target, const int tusTarget);
	int getTriggerTUs(const Edict* shooter, const Edict* target);
	void advance(const Edict* shooter, const int tusShot);
	void reset();
	void notifyClientMove(const Edict* target, int step, bool startMove);
	void notifyClientOnStep(const Edict* target, int step);
	void create(const Edict* shooter);
	void resetTargetList(const Edict* shooter);
	void notifyClientOnShot(const Edict* target, int step);
	void notifyClientRFAborted(const Edict* shooter, const Edict* target, int step);

private:
	ReactionFireTargetList rfData[MAX_RF_DATA];
	ReactionFireTargetList* find (const Edict* shooter);
};

static ReactionFireTargets rft;

/**
 * @brief Initialize the reaction fire table for all entities.
 */
void ReactionFireTargets::init (void)
{
	for (int i = 0; i < MAX_RF_DATA; i++) {
		rfData[i].init();
	}
}

/**
 * @brief Reset the target count in the reaction fire table for all entities.
 */
void ReactionFireTargets::reset (void)
{
	for (int i = 0; i < MAX_RF_DATA; i++) {
		rfData[i].reset();
	}
}

void ReactionFireTargets::notifyClientOnStep (const Edict* target, int step)
{
	for (int i = 0; i < MAX_RF_DATA; i++) {
		ReactionFireTargetList* rfts = &rfData[i];
		if (rfts->entnum == RF_NO_ENTNUM)
			continue;
		const Edict* shooter = G_EdictsGetByNum(rfts->entnum);
		for (int j = 0; j < rfts->count; j++) {
			ReactionFireTarget& t = rfts->targets[j];
			if (t.target != target)
				continue;
			const int tus = std::max(0, target->TU - t.triggerTUs);
			G_EventReactionFireTargetUpdate(*shooter, *target, tus, step);
		}
	}
}

void ReactionFireTargets::notifyClientOnShot (const Edict* target, int tusTarget)
{
	for (int i = 0; i < MAX_RF_DATA; i++) {
		ReactionFireTargetList* rfts = &rfData[i];
		if (rfts->entnum == RF_NO_ENTNUM)
			continue;
		const Edict* shooter = G_EdictsGetByNum(rfts->entnum);
		for (int j = 0; j < rfts->count; j++) {
			ReactionFireTarget& t = rfts->targets[j];
			if (t.target != target)
				continue;
			const int tus = std::max(0, target->TU - tusTarget - t.triggerTUs);
			G_EventReactionFireTargetUpdate(*shooter, *target, tus, MAX_ROUTE);
		}
	}
}

void ReactionFireTargets::notifyClientMove (const Edict* target, int step, bool startMove)
{
	for (int i = 0; i < MAX_RF_DATA; i++) {
		ReactionFireTargetList* rfts = &rfData[i];
		if (rfts->entnum == RF_NO_ENTNUM)
			continue;
		const Edict* shooter = G_EdictsGetByNum(rfts->entnum);
		for (int j = 0; j < rfts->count; j++) {
			if (rfts->targets[j].target != target)
				continue;
			if (startMove) {
				const int tus = std::max(0, target->TU - rfts->targets[j].triggerTUs);
				G_EventReactionFireAddTarget(*shooter, *target, tus, step);
			} else {
				G_EventReactionFireRemoveTarget(*shooter, *target, step);
			}
		}
	}
}

void ReactionFireTargets::notifyClientRFAborted (const Edict* shooter, const Edict* target, int step)
{
		ReactionFireTargetList* rfts = find(shooter);
		assert(rfts);

		for (int i = 0; i < rfts->count; i++) {
			ReactionFireTarget& t = rfts->targets[i];
			if (t.target != target)
				continue;
			G_EventReactionFireAbortShot(*shooter, *target, step);
		}
}

/**
 * @brief Find the given edict's table of reaction fire targets.
 * @param[in] shooter The reaction firing actor
 */
ReactionFireTargetList* ReactionFireTargets::find (const Edict* shooter)
{
	for (int i = 0; i < MAX_RF_DATA; i++) {
		ReactionFireTargetList* rfts = &rfData[i];
		if (rfts->entnum == shooter->getIdNum()) {
			return rfts;
		}
	}
	return nullptr;
}


/**
 * @brief Create a table of reaction fire targets for the given edict.
 * @param[in] shooter The reaction firing actor
 */
void ReactionFireTargets::create (const Edict* shooter)
{
	const ReactionFireTargetList* rfts = find(shooter);

	if (rfts)
		gi.Error("Entity already has rfData");

	for (int i = 0; i < MAX_RF_DATA; i++) {
		ReactionFireTargetList& data = rfData[i];
		if (data.entnum != RF_NO_ENTNUM)
			continue;
		data.entnum = shooter->getIdNum();
		return;
	}

	gi.Error("Not enough rfData");
}

/**
 * @brief Add a reaction fire target for the given shooter.
 * @param[in] shooter The reaction firing actor
 * @param[in] target The potential reaction fire victim
 * @param[in] tusForShot The TUs needed for the shot
 */
void ReactionFireTargets::add (const Edict* shooter, const Edict* target, const int tusForShot)
{
	int i;
	ReactionFireTargetList* rfts = find(shooter);

	assert(rfts);
	assert(target);

	for (i = 0; i < rfts->count; i++) {
		/* check if shooter already knows that target */
		if (rfts->targets[i].target == target)
			return;
	}
	if (i >= MAX_RF_TARGETS)
		return;
	rfts->targets[i].target = target;
	rfts->targets[i].triggerTUs = target->TU - tusForShot;
	rfts->count++;
	G_EventReactionFireAddTarget(*shooter, *target, tusForShot, target->moveinfo.steps - 1);
#if DEBUG_RF
	if (!(G_IsAlien(shooter) || G_IsCivilian(shooter)))
		Com_Printf("S%i: added\n", shooter->number);
#endif
}

/**
 * @brief Remove a reaction fire target for the given shooter.
 * @param[in] shooter The reaction firing actor
 * @param[in] target The potential reaction fire victim
 */
void ReactionFireTargets::remove (const Edict* shooter, const Edict* target)
{
	ReactionFireTargetList* rfts = find(shooter);

	assert(rfts);
	assert(target);

	for (int i = 0; i < rfts->count; i++) {
		ReactionFireTarget& t = rfts->targets[i];
		if (t.target != target)
			continue;

		/* not the last one? */
		if (i != rfts->count - 1) {
			t.target = rfts->targets[rfts->count - 1].target;
			t.triggerTUs = rfts->targets[rfts->count - 1].triggerTUs;
		}
		rfts->count--;
		G_EventReactionFireRemoveTarget(*shooter, *target, target->moveinfo.steps - 1);
#if DEBUG_RF
		if (!(G_IsAlien(shooter) || G_IsCivilian(shooter)))
			Com_Printf("S%i: removed\n", shooter->number);
#endif
	}
}

void ReactionFireTargets::resetTargetList (const Edict* shooter)
{
	ReactionFireTargetList* rfts = find(shooter);
	for (int i = rfts->count - 1; i >= 0; --i)
		remove(shooter, rfts->targets[i].target);

	rfts->reset();
}

/**
 * @brief Check if the given shooter is ready to reaction fire at the given target.
 * @param[in] shooter The reaction firing actor
 * @param[in] target The potential reaction fire victim
 * @return The TUs the target will need to reach until the RF shot goes off.
 */
int ReactionFireTargets::getTriggerTUs (const Edict* shooter, const Edict* target)
{
	const ReactionFireTargetList* rfts = find(shooter);

	if (!rfts)
		return -2;	/* the shooter doesn't aim at anything */

	assert(target);

	for (int i = 0; i < rfts->count; i++) {
		const ReactionFireTarget& t = rfts->targets[i];
		if (t.target == target)
			return t.triggerTUs;
	}

	return -1;	/* the shooter doesn't aim at this target */
}


/**
 * @brief Check if the given shooter is ready to reaction fire at the given target.
 * @param[in] shooter The reaction firing actor
 * @param[in] target The potential reaction fire victim
 * @param[in] tusTarget The TUs the target will need for the shot, 0 for just moving
 */
bool ReactionFireTargets::hasExpired (const Edict* shooter, const Edict* target, const int tusTarget)
{
	const ReactionFireTargetList* rfts = find(shooter);

	if (!rfts)
		return false;	/* the shooter doesn't aim at anything */

	assert(target);

	for (int i = 0; i < rfts->count; i++) {
		const ReactionFireTarget& t = rfts->targets[i];
		if (t.target == target)
			return t.triggerTUs >= target->TU - tusTarget;
	}

	return false;	/* the shooter doesn't aim at this target */
}


/**
 * @brief Increase the triggertime for the next RF shot for all targets of the shooter (after a reaction fire).
 * @param[in] shooter The reaction firing actor
 * @param[in] tusShot The TUs the shooter will need for the next shot
 */
void ReactionFireTargets::advance (const Edict* shooter, const int tusShot)
{
	ReactionFireTargetList* rfts = find(shooter);
	assert(rfts);

	for (int i = 0; i < rfts->count; i++) {
		ReactionFireTarget& t = rfts->targets[i];
		t.triggerTUs -= tusShot;
	}
}

/**
 * @brief free function to initialize the reaction fire table for all entities.
 */
void G_ReactionFireTargetsInit (void)
{
	rft.init();
}

/**
 * @brief free function to create a table of reaction fire targets for the given edict.
 * @param[in] shooter The reaction firing actor
 */
void G_ReactionFireTargetsCreate (const Edict* shooter)
{
	rft.create(shooter);
}

class ReactionFire
{
private:
	bool isEnemy(const Actor* shooter, const Edict* target) const;
	bool canReact(Actor* shooter, const Edict* target) const;
	bool canSee(const Actor* shooter, const Edict* target) const;
	bool shoot(Actor* shooter, const pos3_t at, shoot_types_t type, fireDefIndex_t firemode);
	bool isPossible(Actor* shooter, const Edict* target) const;
public:
	void notifyClientOnStep(const Edict* target, int step);
	bool checkExecution(const Edict* target, int step);
	void updateAllTargets(const Edict* target);
	bool tryToShoot(Actor* shooter, const Edict* target);
	bool isInWeaponRange(const Actor* shooter, const Edict* target, const fireDef_t* fd) const;
	const fireDef_t* getFireDef(const Actor* shooter) const;
	void resetTargets(const Edict* shooter);
	void notifyClientOnShot(const Edict* target, int tusTarget);
};
static ReactionFire rf;

/**
 * @brief Get the fireDef for the RF settings of the shooter.
 * @param[in] shooter The reaction firing actor
 * @return nullptr if something is wrong
 */
const fireDef_t* ReactionFire::getFireDef (const Actor* shooter) const
{
	const FiremodeSettings* fmSetting = &shooter->chr.RFmode;
	if (!fmSetting->isSaneFiremode())
		return nullptr;

	const Item* weapon = shooter->getHandItem(fmSetting->getHand());

	if (weapon && weapon->ammoDef() && weapon->isWeapon() && !weapon->mustReload()) {
		const fireDef_t* fdArray = weapon->getFiredefs();
		if (fdArray == nullptr)
			return nullptr;

		const fireDefIndex_t fmIdx = fmSetting->getFmIdx();
		return &fdArray[fmIdx];
	}
	return nullptr;
}

bool ReactionFire::isInWeaponRange (const Actor* shooter, const Edict* target, const fireDef_t* fd) const
{
	assert(fd);
	return fd->range >= VectorDist(shooter->origin, target->origin);
}

/**
 * @brief Get the weapon firing TUs of the item in the hand of the shooter.
 * @return -1 if no firedef was found for the item or the reaction fire mode is not activated.
 * @param[in] shooter The reaction firing actor
 * @param[in] target The target to check reaction fire for (e.g. check whether the weapon that was marked for
 * using in reaction fire situations can handle the distance between the shooter and the target)
 */
static int G_ReactionFireGetTUsForItem (const Actor* shooter, const Edict* target)
{
	const fireDef_t* fd = rf.getFireDef(shooter);
	if (!fd)
		return -1;

	const int tus = G_ActorGetModifiedTimeForFiredef(shooter, fd, true);

	if (tus <= shooter->TU && rf.isInWeaponRange(shooter, target, fd)) {
		return tus;
	}

	return -1;
}

/**
 * @brief Checks if the currently selected firemode is usable with the defined weapon.
 * @param[in] actor The actor to check the firemode for.
 */
static bool G_ActorHasWorkingFireModeSet (const Edict* actor)
{
	const FiremodeSettings* fmSettings = &actor->chr.RFmode;
	if (!fmSettings->isSaneFiremode())	/* just checks for valid values */
		return false;

	const Item* weapon = actor->getHandItem(fmSettings->getHand());
	if (!weapon)
		return false;
	const fireDef_t* fd = weapon->getFiredefs();
	if (fd == nullptr)
		return false;

	if (fd->obj->weapons[fd->weapFdsIdx] == fmSettings->getWeapon()
		&& fmSettings->getFmIdx() < fd->obj->numFiredefs[fd->weapFdsIdx]) {
		return true;
	}

	return false;
}

/**
 * @brief Updates the reaction fire settings in case something was moved into a hand or from a hand
 * that would make the current settings invalid
 * @param[in,out] actor The actor edict to check the settings for
 * @param[in] fmIdx The fire mode index that should be used for reaction fire
 * @param[in] hand The hand that should be used for reaction fire
 * @param[in] od The object/weapon for the reaction fire
 */
void G_ReactionFireSettingsUpdate (Actor* actor, fireDefIndex_t fmIdx, actorHands_t hand, const objDef_t* od)
{
	actor->chr.RFmode.set(hand, fmIdx, od);	/* FiremodeSettings */

	if (!G_ActorHasWorkingFireModeSet(actor)) {
		/* Disable reaction fire if no valid firemode was found. */
		G_ClientStateChange(actor->getPlayer(), actor, ~STATE_REACTION, false);
		G_EventReactionFireChange(*actor);
		return;
	}

	G_EventReactionFireChange(*actor);

	/* If reaction fire is active, update the reserved TUs */
	if (actor->isReaction()) {
		G_ReactionFireSettingsReserveTUs(actor);
	}
}

/**
 * @brief Checks whether an actor has enough TUs left to activate reaction fire.
 * @param[in] ent The actors edict to check for TUs for
 * @return @c true if the given actor has enough TUs left to activate reaction fire, @c false otherwise.
 */
static bool G_ActorHasEnoughTUsReactionFire (const Edict* ent)
{
	const int TUs = G_ActorGetTUForReactionFire(ent);
	const chrReservations_t* res = &ent->chr.reservedTus;
	return ent->TU - TUs >= res->shot + res->crouch;
}

/**
 * @param ent The actor to set the reaction fire for
 * @return @c true if the needed settings could have been made or settings are
 * already valid, @c false otherwise.
 */
static bool G_ReactionFireSettingsSetDefault (Edict* ent)
{
	if (G_ActorHasWorkingFireModeSet(ent))
		return true;

	actorHands_t hand = ACTOR_HAND_RIGHT;
	const Item* item = ent->getHandItem(hand);
	if (!item) {
		hand = ACTOR_HAND_LEFT;
		item = ent->getHandItem(hand);
	}

	if (!item)
		return false;

	const objDef_t* weapon = item->getReactionFireWeaponType();
	if (!weapon)
		return false;

	ent->chr.RFmode.set(hand, 0, weapon);	/* no special firemode */

	if (!G_ActorHasWorkingFireModeSet(ent))
		return false;

	if (!G_IsAI(ent))
		G_EventReactionFireChange(*ent);

	return true;
}

/**
 * @brief Checks whether the actor is allowed to activate reaction fire and will informs the player about
 * the reason if this would not work.
 * @param[in] ent The actor to check
 * @return @c true if the actor is allowed to activate it, @c false otherwise
 */
static bool G_ReactionFireCanBeEnabled (const Edict* ent)
{
	/* check ent is a suitable shooter */
	if (!ent->inuse || !G_IsLivingActor(ent))
		return false;

	if (G_MatchIsRunning() && ent->getTeam() != level.activeTeam)
		return false;

	/* actor may not carry weapons at all - so no further checking is needed */
	if (!ent->chr.teamDef->weapons)
		return false;

	if (!ent->chr.inv.holdsReactionFireWeapon()) {
		G_ClientPrintf(ent->getPlayer(), PRINT_HUD, _("No reaction fire enabled weapon."));
		return false;
	}

	if (!G_ActorHasWorkingFireModeSet(ent)) {
		G_ClientPrintf(ent->getPlayer(), PRINT_HUD, _("No fire mode selected for reaction fire."));
		return false;
	}

	if (!G_ActorHasEnoughTUsReactionFire(ent)) {
		G_ClientPrintf(ent->getPlayer(), PRINT_HUD, _("Not enough TUs left for activating reaction fire."));
		return false;
	}

	return true;
}

/**
 * @brief Set the reaction fire TU reservation for an actor
 * @param[in,out] ent The actor edict to set the TUs for
 * @return @c true if TUs for reaction fire were reserved, @c false if the reservation was set
 * back to @c 0
 */
bool G_ReactionFireSettingsReserveTUs (Actor* ent)
{
	if (G_ReactionFireSettingsSetDefault(ent) && G_ReactionFireCanBeEnabled(ent)) {
		const int TUs = G_ActorGetTUForReactionFire(ent);
		/* Enable requested reaction fire. */
		G_ActorReserveTUs(ent, TUs, ent->chr.reservedTus.shot, ent->chr.reservedTus.crouch);
		return true;
	}

	G_ActorReserveTUs(ent, 0, ent->chr.reservedTus.shot, ent->chr.reservedTus.crouch);
	return false;
}

inline bool ReactionFire::isPossible (Actor* shooter, const Edict* target) const
{
	return isEnemy(shooter, target) && canReact(shooter, target) && canSee(shooter, target);
}

/**
 * @brief Check whether we want to shoot at the target.
 * @param[in] shooter The entity that might be firing
 * @param[in] target The entity that might be fired at
 */
bool ReactionFire::isEnemy (const Actor* shooter, const Edict* target) const
{
	/* an entity can't reaction fire at itself */
	if (shooter == target)
		return false;

	/* Don't react in your own turn */
	if (shooter->getTeam() == level.activeTeam)
		return false;

	if (G_IsDead(target))
		return false;

	/* If reaction fire is triggered by a friendly unit
	 * and the shooter is still sane, don't shoot;
	 * well, if the shooter isn't sane anymore... */
	if (G_IsCivilian(target) || target->isSameTeamAs(shooter))
		if (!shooter->isShaken() || (float) shooter->morale / mor_shaken->value > frand())
			return false;

	return true;
}

/**
 * @brief Check whether shooter can reaction fire at target at all.
 * @param[in] shooter The entity that might be firing
 * @param[in] target The entity that might be fired at
 */
bool ReactionFire::canReact (Actor* shooter, const Edict* target) const
{
	/* shooter can't use RF if is in STATE_DAZED (flashbang impact) */
	if (shooter->isDazed())
		return false;

	/* check shooter has reaction fire enabled */
	if (!shooter->isReaction())
		return false;

	/* check shooter has weapon in RF hand */
	if (!shooter->getHandItem(shooter->chr.RFmode.getHand())) {
		/* print character info if this happens, for now */
		gi.DPrintf("Reaction fire enabled but no weapon for hand (name=%s,entnum=%i,hand=%i,fmIdx=%i)\n",
				shooter->chr.name, shooter->getIdNum(), shooter->chr.RFmode.getHand(), shooter->chr.RFmode.getFmIdx());
		shooter->removeReaction();
		return false;
	}
	return true;
}

/**
 * @brief Check whether shooter can see his target well enough
 * @param[in] shooter The entity that might be firing
 * @param[in] target The entity that might be fired at
 */
bool ReactionFire::canSee (const Actor* shooter, const Edict* target) const
{
	if (!G_IsVisibleForTeam(target, shooter->getTeam()))
		return false;

	/* check in range and visible */
	const int spotDist = G_VisCheckDist(shooter);
	if (VectorDistSqr(shooter->origin, target->origin) > spotDist * spotDist)
		return false;

	const bool frustum = G_FrustumVis(shooter, target->origin);
	if (!frustum)
		return false;

	const float actorVis = G_ActorVis(shooter, target, true);
	if (actorVis < 0.1)
		return false;

	return true;
}

/**
 * @brief Check whether 'target' has just triggered any new reaction fire
 * @param[in] target The entity triggering fire
 */
void ReactionFire::updateAllTargets (const Edict* target)
{
	Actor* shooter = nullptr;

	/* check all possible shooters */
	while ((shooter = G_EdictsGetNextLivingActor(shooter))) {
		/* check whether reaction fire is possible (friend/foe, LoS) */
		if (isPossible(shooter, target)) {
			const int TUs = G_ReactionFireGetTUsForItem(shooter, target);
			if (TUs < 0)
				continue;	/* no suitable weapon */
			rft.add(shooter, target, TUs);
		} else {
			rft.remove(shooter, target);
		}
	}
}

void ReactionFire::resetTargets (const Edict* shooter)
{
	rft.resetTargetList(shooter);
}

/**
 * @brief Perform the reaction fire shot
 * @param[in] shooter The actor that is trying to shoot
 * @param[in] at Position to fire on.
 * @param[in] type What type of shot this is (left, right reaction-left etc...).
 * @param[in] firemode The firemode index of the ammo for the used weapon (objDef.fd[][x])  .
 * @return true if everything went ok (i.e. the shot(s) where fired ok), otherwise false.
 * @sa G_ClientShoot
 */
bool ReactionFire::shoot (Actor* shooter, const pos3_t at, shoot_types_t type, fireDefIndex_t firemode)
{
	/* this is the max amount of friendly units that were hit during the mock calculation */
	int maxff;

	if (shooter->isInsane())
		maxff = 100;
	else if (shooter->isRaged())
		maxff = 60;
	else if (shooter->isPanicked())
		maxff = 30;
	else if (shooter->isShaken())
		maxff = 15;
	else
		maxff = 5;

	/* calculate the mock values - e.g. how many friendly units we would hit
	 * when opening the reaction fire */
	const int minhit = 30;
	shot_mock_t mock;
	const Player& player = shooter->getPlayer();
	for (int i = 0; i < 100; i++)
		if (!G_ClientShoot(player, shooter, at, type, firemode, &mock, false, 0))
			break;

	const int ff = mock.friendCount + (G_IsAlien(shooter) ? 0 : mock.civilian);
	if (ff <= maxff && mock.enemyCount >= minhit)
		return G_ClientShoot(player, shooter, at, type, firemode, nullptr, false, 0);

	return false;
}

/**
 * @brief Resolve the reaction fire for an entity, this checks that the entity can fire and then takes the shot
 * @param[in] shooter The entity using reaction fire
 * @param[in] target The victim of the reaction fire
 * @return true if the entity fired, false otherwise
 */
bool ReactionFire::tryToShoot (Actor* shooter, const Edict* target)
{
	/* check for valid target */
	assert(target);

	/* shooter can't take a reaction shot if it's not possible - and check that
	 * the target is still alive */
	if (!isPossible(shooter, target)) {
		rft.remove(shooter, target);
		return false;
	}

	/* take the shot */
	const bool tookShot = rf.shoot(shooter, target->pos, ST_RIGHT_REACTION, shooter->chr.RFmode.getFmIdx());

	if (tookShot) {
		/* clear any shakenness */
		shooter->removeShaken();
	}

	return tookShot;
}

void ReactionFire::notifyClientOnShot (const Edict* target, int tusTarget)
{
	rft.notifyClientOnShot(target, tusTarget);
}

void ReactionFire::notifyClientOnStep (const Edict* target, int step)
{
	rft.notifyClientOnStep(target, step);
}

/**
 * @brief Check all entities to see whether target has caused reaction fire to resolve.
 * @param[in] target The entity that might be resolving reaction fire
 * @param[in] step The number of the step in the move we are checking reactions for
 * @returns whether any entity fired (or would fire) upon target
 * @sa G_ReactionFireOnMovement
 * @sa G_ReactionFirePostShot
 */
bool ReactionFire::checkExecution (const Edict* target, int step)
{
	Actor* shooter = nullptr;
	bool fired = false;

	/* check all possible shooters */
	while ((shooter = G_EdictsGetNextLivingActor(shooter))) {
		const int tus = G_ReactionFireGetTUsForItem(shooter, target);
		/* indicates an RF weapon is there */
		if (tus <= 1)
			continue;
		if (!rft.hasExpired(shooter, target, 0))
			continue;
		if (!rf.tryToShoot(shooter, target)) {
			G_ReactionFireNotifyClientRFAborted(shooter, target, step);
			continue;
		}
		rft.advance(shooter, tus);
		fired |= true;
	}
	return fired;
}

#if DEBUG_RF
/**
 * @brief Prints some reaction fire data to the console
 * @param[in] target The target entity
 */
static void G_ReactionFirePrintSituation (Edict* target)
{
	if (!G_IsAlien(target))
		return;

	Com_Printf("Alien %i at %i/%i/%i TU:%i\n", target->number, target->pos[0], target->pos[1], target->pos[2], target->TU);

	Actor* shooter = nullptr;
	/* check all possible shooters */
	while ((shooter = G_EdictsGetNextLivingActor(shooter))) {
		if (G_IsAlien(shooter) || G_IsCivilian(shooter))
			continue;
		char msgHdr[100];
		Com_sprintf(msgHdr, sizeof(msgHdr), "S%i: at %i/%i/%i RF: ", shooter->number, shooter->pos[0], shooter->pos[1], shooter->pos[2]);
		int ttus = rft.getTriggerTUs(shooter, target);
		if (ttus == -2)
			Com_Printf("%s not initialized\n", msgHdr);
		if (ttus == -1)
			Com_Printf("%s not aiming\n", msgHdr);
		else if (rft.hasExpired(shooter, target, 0))
			Com_Printf("expired\n", msgHdr);
		else
			Com_Printf("%s not yet: %i\n", msgHdr, ttus);
	}
}
#endif

/**
 * @brief Called when 'target' moves, possibly triggering or resolving reaction fire
 * @param[in] target The target entity
 * @param[in] step The number of the step in the move we are checking reactions for
 * @return true If any shots were (or would be) taken
 * @sa G_ClientMove
 */
bool G_ReactionFireOnMovement (Actor* target, int step)
{
#if DEBUG_RF
	G_ReactionFirePrintSituation(target);
#endif
	rf.notifyClientOnStep(target, step);

	/* Check to see whether this resolves any reaction fire */
	const bool fired = rf.checkExecution(target, step);

	/* Check to see whether this triggers any reaction fire */
	rf.updateAllTargets(target);

	return fired;
}

static void G_ReactionFireNotifyClientStartShot (const Edict* target)
{
	rft.notifyClientMove(target, MAX_ROUTE, true);
}

static void G_ReactionFireNotifyClientEndShot (const Edict* target)
{
	rft.notifyClientMove(target, MAX_ROUTE, false);
}

/**
 * @brief Called when 'target' is about to shoot, this forces a 'draw' to decide who gets the first shot
 * @param[in] target The entity about to shoot
 * @param[in] fdTime The TU of the shot
 * @sa G_ClientShoot
 */
void G_ReactionFirePreShot (const Actor* target, const int fdTime)
{
	bool repeat = true;

	/* Check to see whether this triggers any reaction fire */
	G_ReactionFireNotifyClientStartShot(target);
	rf.updateAllTargets(target);
	rf.notifyClientOnShot(target, fdTime);

	/* if any reaction fire occurs, we have to loop through all entities again to allow
	 * multiple (fast) RF snap shots before a (slow) aimed shot from the target occurs. */
	while (repeat) {
		Actor* shooter = nullptr;
		repeat = false;
		/* check all ents to see who wins and who loses a draw */
		while ((shooter = G_EdictsGetNextLivingActor(shooter))) {
			const int entTUs = G_ReactionFireGetTUsForItem(shooter, target);
			/* indicates an RF weapon is there */
			if (entTUs <= 1)
				continue;
			if (!rft.hasExpired(shooter, target, fdTime))
				continue;
			if (!rf.tryToShoot(shooter, target)) {
				G_ReactionFireNotifyClientRFAborted(shooter, target, MAX_ROUTE);
				continue;
			}
			repeat = true;
			rft.advance(shooter, fdTime);
		}
	}
}

/**
 * @brief Removes the given target from the reaction fire lists
 * @param[in] target The target to remove from the lists
 */
void G_ReactionFireOnDead (const Actor* target)
{
	assert(G_IsDead(target));
	rf.updateAllTargets(target);
	rf.resetTargets(target);
}

/**
 * @brief Called after 'target' has fired, this might trigger more reaction fire or resolve outstanding reaction fire (because target is out of time)
 * @param[in] target The entity that has just fired
 * @sa G_ClientShoot
 */
void G_ReactionFirePostShot (Actor* target)
{
	/* Check to see whether this resolves any reaction fire */
	rf.notifyClientOnShot(target, 0);
	rf.checkExecution(target, MAX_ROUTE);
	G_ReactionFireNotifyClientEndShot(target);
}

/**
 * @brief Called at the end of turn, all outstanding reaction fire is resolved
 * @sa G_ClientEndRound
 */
void G_ReactionFireOnEndTurn (void)
{
	/* we explicitly do nothing at end of turn, just reset the table */
	rft.reset();
}

/**
 * @brief Guess! Reset all "shaken" states on end of turn?
 * @note Normally called on end of turn.
 * @sa G_ClientStateChange
 * @param[in] team Index of team to loop through.
 */
void G_ReactionFireReset (int team)
{
	Actor* actor = nullptr;

	while ((actor = G_EdictsGetNextLivingActorOfTeam(actor, team))) {
		actor->removeShaken();
	}
}

void G_ReactionFireNotifyClientStartMove (const Actor* target)
{
	/* note that this is sent _before_ the actual move event, so we can't use the step number */
	rft.notifyClientMove(target, MAX_ROUTE, true);
}

void G_ReactionFireNotifyClientEndMove (const Actor* target)
{
	rft.notifyClientMove(target, target->moveinfo.steps - 1, false);
}

void G_ReactionFireNotifyClientRFAborted (const Actor* shooter, const Edict* target, int step)
{
	rft.notifyClientRFAborted(shooter, target, step);
}
