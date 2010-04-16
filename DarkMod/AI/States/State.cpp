/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision$
 * $Date$
 * $Author$
 *
 ***************************************************************************/

#include "../idlib/precompiled.h"
#pragma hdrstop

static bool init_version = FileVersionList("$Id$", init_version);

#include "State.h"
#include "../Memory.h"
#include "../Tasks/SingleBarkTask.h"
#include "../Tasks/GreetingBarkTask.h"
#include "../Tasks/HandleDoorTask.h"
#include "../Tasks/HandleElevatorTask.h"
#include "../../AIComm_Message.h"
#include "../../StimResponse/StimResponse.h"
#include "SearchingState.h"
#include "CombatState.h"
#include "BlindedState.h"
#include "SwitchOnLightState.h"
#include "FailedKnockoutState.h"

#include "../../BinaryFrobMover.h"
#include "../../FrobDoor.h"
#include "../../AbsenceMarker.h"

namespace ai
{


//----------------------------------------------------------------------------------------
// The following strings define classes of person, these are used if AIUse is AIUSE_PERSON 

// This is the key value
#define PERSONTYPE_KEY				"personType"

// And these are values in use, add to this list as needed
#define PERSONTYPE_GENERIC			"PERSONTYPE_GENERIC"
#define PERSONTYPE_NOBLE			"PERSONTYPE_NOBLE"
#define PERSONTYPE_CITYWATCH		"PERSONTYPE_CITYWATCH"
#define PERSONTYPE_MERC_PROGUARD	"PERSONTYPE_MERC_PROGUARD"
#define PERSONTYPE_BUILDER			"PERSONTYPE_BUILDER"
#define PERSONTYPE_PAGAN			"PERSONTYPE_PAGAN"
#define PERSONTYPE_THIEF			"PERSONTYPE_THIEF"
#define PERSONTYPE_PRIEST			"PERSONTYPE_PRIEST"
#define PERSONTYPE_ELITE			"PERSONTYPE_ELITE"

//----------------------------------------------------------------------------------------
// The following strings define genders of person, these are used if AIUse is AIUSE_PERSON 
// I don't want to get into the politics of gender identity here, this is just because the recorded
// voices will likely be in gendered languages.  As such, I'm just including the categories
// that are involved in word gender selection in many languages.
#define PERSONGENDER_KEY		"personGender"

#define PERSONGENDER_MALE		"PERSONGENDER_MALE"
#define PERSONGENDER_FEMALE		"PERSONGENDER_FEMALE"
#define PERSONGENDER_UNKNOWN	"PERSONGENDER_UNKNOWN"

const int MIN_TIME_BETWEEN_GREETING_CHECKS = 20000; // msecs
const float CHANCE_FOR_GREETING = 0.3f; // 30% chance for greeting

//----------------------------------------------------------------------------------------
// The following defines a key that should be non-0 if the device should be closed
#define AIUSE_SHOULDBECLOSED_KEY		"shouldBeClosed"

void State::Init(idAI* owner)
{
	_owner = owner;
	_alertLevelDecreaseRate = 0;

	// Load the value from the spawnargs to avoid looking it up each frame
	owner->GetMemory().deadTimeAfterAlertRise = owner->spawnArgs.GetInt("alert_decrease_deadtime", "300");
}

bool State::CheckAlertLevel(idAI* owner)
{
	return true; // always true by default
}

void State::SetOwner(idAI* owner)
{
	_owner = owner;
}

void State::UpdateAlertLevel()
{
	idAI* owner = _owner.GetEntity();
	int currentTime = gameLocal.time;
	int thinkDuration = currentTime - owner->m_lastThinkTime;

	Memory& memory = owner->GetMemory();
	
	// angua: alert level stays for a short time before starting to decrease
	if (currentTime >= memory.lastAlertRiseTime + memory.deadTimeAfterAlertRise && 
		owner->AI_AlertLevel > 0)
	{
		float decrease = _alertLevelDecreaseRate * MS2SEC(thinkDuration);
		float newAlertLevel = owner->AI_AlertLevel - decrease;
		owner->SetAlertLevel(newAlertLevel);
	}
}


// Save/Restore methods
void State::Save(idSaveGame* savefile) const
{
	_owner.Save(savefile);

	savefile->WriteFloat(_alertLevelDecreaseRate);
}

void State::Restore(idRestoreGame* savefile)
{
	_owner.Restore(savefile);

	savefile->ReadFloat(_alertLevelDecreaseRate);
}

void State::OnVisualAlert(idActor* enemy)
{
	assert(enemy != NULL);

	idAI* owner = _owner.GetEntity();
	assert(owner != NULL);

	if (owner->AI_DEAD || owner->AI_KNOCKEDOUT)
	{
		return;
	}

	Memory& memory = owner->GetMemory();

	memory.alertClass = EAlertVisual;
	memory.alertType = EAlertTypeSuspicious;
	memory.alertPos = owner->GetVisDir();
	memory.alertRadius = VISUAL_ALERT_RADIUS;
	memory.alertSearchVolume = VISUAL_SEARCH_VOLUME;
	memory.alertSearchExclusionVolume.Zero();
	
	// set the flag back (greebo: Is this still necessary?)
	owner->AI_VISALERT = false;

	// Is this alert far enough away from the last one we reacted to to
	// consider it a new alert? Visual alerts are highly compelling and
	// are always considered new
	idVec3 newAlertDeltaFromLastOneSearched(memory.alertPos - memory.lastAlertPosSearched);
	float alertDeltaLengthSqr = newAlertDeltaFromLastOneSearched.LengthSqr();
	
	if (alertDeltaLengthSqr > memory.alertSearchVolume.LengthSqr())
	{
		// This is a new alert // SZ Dec 30, 2006
		// Note changed this from thresh_2 to thresh_3 to match thresh designers intentions
		if (owner->AI_AlertLevel >= owner->thresh_3)
		{
			// We are in searching mode or we are switching to it, handle this new incoming alert

			// Visual stimuli are locatable enough that we should
			// search the exact stim location first
			memory.stimulusLocationItselfShouldBeSearched = true;
			
			// greebo: TODO: Each incoming stimulus == evidence of intruders?
			// One more piece of evidence of something out of place
			memory.countEvidenceOfIntruders++;
		
			// Do new reaction to stimulus
			memory.alertedDueToCommunication = false;

			// Restart the search, in case we're already searching
			memory.restartSearchForHidingSpots = true;
		}	
	} // Not too close to last stimulus or is visual stimulus
}

void State::OnTactileAlert(idEntity* tactEnt)
{
	assert(tactEnt != NULL); // don't take NULL entities

	idAI* owner = _owner.GetEntity();
	assert(owner != NULL);

	if (owner->AI_KNOCKEDOUT || owner->AI_DEAD)
	{
		return;
	}

	// If this is a projectile, fire the corresponding event
	if (tactEnt->IsType(idProjectile::Type))
	{
		OnProjectileHit(static_cast<idProjectile*>(tactEnt));
	}
	else 
	{
		/**
		* FIX: They should not try to MOVE to the tactile alert position
		* because if it's above their head, they can't get to it and will
		* just run in circles.  
		*
		* Instead, turn to this position manually, then set the search target
		* to the AI's own origin, to execute a search starting from where it's standing
		*
		* TODO later: Predict where the thrown object might have come from, and search
		* in that direction (requires a "directed search" algorithm)
		*/
		if (owner->IsEnemy(tactEnt)) // also checks for NULL pointers
		{
			owner->Event_SetEnemy(tactEnt);

			Memory& memory = owner->GetMemory();
			memory.alertClass = EAlertTactile;
			memory.alertType = EAlertTypeEnemy;
			memory.alertPos = owner->GetPhysics()->GetOrigin();
			memory.alertRadius = TACTILE_ALERT_RADIUS;
			memory.alertSearchVolume = TACTILE_SEARCH_VOLUME;
			memory.alertSearchExclusionVolume.Zero();

			// execute the turn manually here
			owner->TurnToward(memory.alertPos);
			
			owner->AI_TACTALERT = false;
		}
	}
}

void State::OnProjectileHit(idProjectile* projectile)
{
	idAI* owner = _owner.GetEntity();

	if (owner->AI_DEAD || owner->AI_KNOCKEDOUT)
	{
		return;
	}

	if (owner->AI_AlertLevel <= (owner->thresh_5 - 0.1f))
	{
		// Set the alert level right below combat threshold
		owner->SetAlertLevel(owner->thresh_5 - 0.1f);

		// The owner will start to search, setup the parameters
		Memory& memory = owner->GetMemory();

		memory.alertClass = EAlertTactile;
		memory.alertType = EAlertTypeDamage;

		idVec3 projVel = projectile->GetPhysics()->GetLinearVelocity();
		projVel.NormalizeFast();

		memory.alertPos = owner->GetPhysics()->GetOrigin() - projVel * 300;
		memory.alertPos.x += 200 * gameLocal.random.RandomFloat() - 100;
		memory.alertPos.y += 200 * gameLocal.random.RandomFloat() - 100;
		memory.alertRadius = LOST_ENEMY_ALERT_RADIUS;
		memory.alertSearchVolume = LOST_ENEMY_SEARCH_VOLUME;
		memory.alertSearchExclusionVolume.Zero();
	}
}

void State::OnAudioAlert() 
{
	idAI* owner = _owner.GetEntity();
	assert(owner != NULL);

	if (owner->AI_DEAD || owner->AI_KNOCKEDOUT)
	{
		return;
	}

	Memory& memory = owner->GetMemory();

	memory.alertClass = EAlertAudio;
	memory.alertType = EAlertTypeSuspicious;
	memory.alertPos = owner->GetSndDir();
	memory.lastAudioAlertTime = gameLocal.time;

	// Search within radius of stimulus that is 1/3 the distance from the
	// observer to the point at the time heard
	float distanceToStim = (owner->GetPhysics()->GetOrigin() - memory.alertPos).LengthFast();

	// greebo: Apply a certain fuzziness to the audio alert position
	// 200 units distance corresponds to 50 units fuzziness in X/Y direction
	memory.alertPos += idVec3(
		(gameLocal.random.RandomFloat() - 0.5f)*AUDIO_ALERT_FUZZINESS,
		(gameLocal.random.RandomFloat() - 0.5f)*AUDIO_ALERT_FUZZINESS,
		0 // no fuzziness in z-direction
	) * distanceToStim / 400.0f;

	float searchVolModifier = distanceToStim / 600.0f;
	if (searchVolModifier < 0.4f)
	{
		searchVolModifier = 0.4f;
	}

	memory.alertRadius = AUDIO_ALERT_RADIUS;
	memory.alertSearchVolume = AUDIO_SEARCH_VOLUME * searchVolModifier;
	memory.alertSearchExclusionVolume.Zero();
	
	// Reset the flag (greebo: is this still necessary?)
	owner->AI_HEARDSOUND = false;
	
	memory.stimulusLocationItselfShouldBeSearched = true;
}

void State::OnBlindStim(idEntity* stimSource, bool skipVisibilityCheck)
{
	idAI* owner = _owner.GetEntity();

	// Don't react if we are already blind
	if (owner->AI_DEAD || owner->AI_KNOCKEDOUT || owner->GetAcuity("vis") == 0)
	{
		return;
	}

	// greebo: We don't check for alert type weights here, flashbombs are "top priority"

	Memory& memory = owner->GetMemory();

	memory.alertClass = EAlertVisual;
	memory.alertedDueToCommunication = false;
	memory.alertPos = stimSource->GetPhysics()->GetOrigin();
	memory.alertRadius = 200;
	memory.alertType = EAlertTypeWeapon;

	if (!skipVisibilityCheck) 
	{
		// Perform visibility check
		// Check FOV first
		if (owner->CheckFOV(stimSource->GetPhysics()->GetOrigin()))
		{
			// FOV check passed, check occlusion (skip lighting check)
			if (owner->CanSeeExt(stimSource, false, false))
			{
				// Success, AI is blinded
				DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("AI can see the flash, switching to BlindedState.\r");
				owner->GetMind()->PushState(STATE_BLINDED);
			}
			else
			{
				DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("AI cannot see the flash.\r");
			}
		}
		else
		{
			// greebo: FOV check might have failed, still consider near explosions of flashbombs
			// as some AI might be so close to the player that dropping a flashbomb goes outside FOV
			idVec3 distVec = (stimSource->GetPhysics()->GetOrigin() - owner->GetPhysics()->GetOrigin());
			float dist = distVec.NormalizeFast();
			
			// Check if the distance is within 100 units and the explosion is in front of us
			if (dist < 100.0f && distVec * owner->viewAxis.ToAngles().ToForward() > 0)
			{
				DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("AI blinded by flash at its feet, switching to BlindedState.\r");
				owner->GetMind()->PushState(STATE_BLINDED);
			}
			else
			{
				DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("AI cannot see the flash.\r");
			}
		}
	}
	else 
	{
		DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("Visibility check for flash skipped, switching to BlindedState.\r");

		// Skip visibility check
		owner->GetMind()->PushState(STATE_BLINDED);
	}
}

void State::OnVisualStim(idEntity* stimSource)
{
	if (cv_ai_opt_novisualstim.GetBool()) 
	{
		return;
	}

	idAI* owner = _owner.GetEntity();
	if (owner == NULL)
	{
		// Owner might not be initialised, serviceEvents is called after Mind::Think()
		return;
	}

	// Don't respond to NULL entities or when dead/knocked out/blind and no enemy in sight
	if (stimSource == NULL || 
		owner->AI_KNOCKEDOUT || owner->AI_DEAD || owner->GetAcuity("vis") == 0)
	{
		return;
	}

	// Get AI use of the stim
	idStr aiUse = stimSource->spawnArgs.GetString("AIUse");

	// Only respond if we can actually see it
	if (aiUse == AIUSE_LIGHTSOURCE || aiUse == AIUSE_BROKEN_ITEM)
	{
		// Special case for lights, we know it is off if there is no light. Also we can notice it
		// if we are not looking right at it.
		if (!owner->CanSeeExt(stimSource, false, false))
		{
			return;
		}
	}
	else if (!owner->CanSee(stimSource, true))
	{
		//DEBUG_PRINT ("I can't see the " + aiUse);
		return;
	}

	// Random chance
	float chance(gameLocal.random.RandomFloat());
	float chanceToNotice(0);
		
	if (aiUse == AIUSE_PERSON)
	{
		chanceToNotice = owner->spawnArgs.GetFloat("chanceNoticePerson");
		if (chance < chanceToNotice)
		{
			OnPersonEncounter(stimSource, owner);
		}
		return;
	}

	// Not a person stim, ignore all other stims if enenemy != NULL
	if (owner->GetEnemy() != NULL)
	{
		return;
	}

	if (aiUse == AIUSE_WEAPON)
	{
		chanceToNotice = owner->spawnArgs.GetFloat("chanceNoticeWeapon");
		if (chance < chanceToNotice && ShouldProcessAlert(EAlertTypeWeapon))
		{
			OnVisualStimWeapon(stimSource, owner);
		}
	}
	else if (aiUse == AIUSE_BLOOD_EVIDENCE)
	{
		chanceToNotice = owner->spawnArgs.GetFloat("chanceNoticeBlood");
		if (chance < chanceToNotice && ShouldProcessAlert(EAlertTypeBlood))
		{
			OnVisualStimBlood(stimSource, owner);
		}
	}
	else if (aiUse == AIUSE_LIGHTSOURCE)
	{
		chanceToNotice = owner->spawnArgs.GetFloat("chanceNoticeLight");
		if (chance < chanceToNotice && ShouldProcessAlert(EAlertTypeLightSource))
		{
			OnVisualStimLightSource(stimSource, owner);
		}
	}
	else if (aiUse == AIUSE_MISSING_ITEM_MARKER)
	{
		chanceToNotice = owner->spawnArgs.GetFloat("chanceNoticeMissingItem");
		if (chance < chanceToNotice && ShouldProcessAlert(EAlertTypeMissingItem))
		{
			OnVisualStimMissingItem(stimSource, owner);
		}
	}
	else if (aiUse == AIUSE_BROKEN_ITEM)
	{
		chanceToNotice = owner->spawnArgs.GetFloat("chanceNoticeBrokenItem");
		if (chance < chanceToNotice && ShouldProcessAlert(EAlertTypeBrokenItem))
		{
			OnVisualStimBrokenItem(stimSource, owner);
		}
	}
	else if (aiUse == AIUSE_DOOR)
	{
		chanceToNotice = owner->spawnArgs.GetFloat("chanceNoticeDoor");
		if (chance < chanceToNotice && ShouldProcessAlert(EAlertTypeDoor))
		{
			OnVisualStimDoor(stimSource, owner);
		}
	}
}

bool State::ShouldProcessAlert(EAlertType newAlertType)
{
	idAI* owner = _owner.GetEntity();
	assert(owner != NULL);

	// Memory shortcut
	Memory& memory = owner->GetMemory();
	
	if (owner->alertTypeWeight[memory.alertType] <= owner->alertTypeWeight[newAlertType])
	{
		return true;
	}

	return false;
}

void State::OnVisualStimWeapon(idEntity* stimSource, idAI* owner)
{
	assert(stimSource != NULL && owner != NULL); // must be fulfilled

	// Memory shortcut
	Memory& memory = owner->GetMemory();

	// We've seen this object, don't respond to it again
	stimSource->ResponseIgnore(ST_VISUAL, owner);

	if (stimSource->IsType(idWeapon::Type))
	{
		// Is it a friendly weapon?  To find out we need to get its owner.
		idActor* objectOwner = static_cast<idWeapon*>(stimSource)->GetOwner();
		
		if (owner->IsFriend(objectOwner))
		{
			DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("Ignoring visual stim from weapon with friendly owner\r");
			return;
		}
	}
	
	// Vocalize that see something out of place
	gameLocal.Printf("Hmm, that isn't right! A weapon!\n");
	if (owner->AI_AlertLevel < owner->thresh_5 &&
		gameLocal.time - memory.lastTimeVisualStimBark >= MINIMUM_SECONDS_BETWEEN_STIMULUS_BARKS)
	{
		memory.lastTimeVisualStimBark = gameLocal.time;
		owner->commSubsystem->AddCommTask(
			CommunicationTaskPtr(new SingleBarkTask("snd_foundWeapon"))
		);
	}

	// TWO more piece of evidence of something out of place: A weapon is not a good thing
	memory.countEvidenceOfIntruders += 2;

	// Raise alert level
	if (owner->AI_AlertLevel < owner->thresh_4 - 0.1f)
	{
		owner->SetAlertLevel(owner->thresh_4 - 0.1f);
	}
	
	memory.alertPos = stimSource->GetPhysics()->GetOrigin();
	memory.alertClass = EAlertVisual;
	memory.alertType = EAlertTypeWeapon;

	// Do search as if there is an enemy that has escaped
	memory.alertRadius = LOST_ENEMY_ALERT_RADIUS;
	memory.alertSearchVolume = LOST_ENEMY_SEARCH_VOLUME; 
	memory.alertSearchExclusionVolume.Zero();
	
	owner->AI_VISALERT = false;
	
	// Do new reaction to stimulus
	memory.stimulusLocationItselfShouldBeSearched = true;
	memory.investigateStimulusLocationClosely = true; // deep investigation
	memory.alertedDueToCommunication = false;
}

void State::OnPersonEncounter(idEntity* stimSource, idAI* owner)
{
	assert(stimSource != NULL && owner != NULL); // must be fulfilled

	Memory& memory = owner->GetMemory();

	bool ignoreStimulusFromNowOn = true;
	
	if (!stimSource->IsType(idActor::Type)) return; // No Actor, quit

	// Hard-cast the stimsource onto an actor 
	idActor* other = static_cast<idActor*>(stimSource);	

	// Are they dead or unconscious?
	if (other->health <= 0) 
	{
		if (ShouldProcessAlert(EAlertTypeDeadPerson))
		{
			// React to finding body
			ignoreStimulusFromNowOn = OnDeadPersonEncounter(other, owner);
			if (ignoreStimulusFromNowOn)
			{
				owner->TactileIgnore(stimSource);
			}
		}
		else
		{
			ignoreStimulusFromNowOn = false;
		}
	}
	else if (other->IsKnockedOut())
	{
		if (ShouldProcessAlert(EAlertTypeUnconsciousPerson))
		{
			// React to finding unconscious person
			ignoreStimulusFromNowOn = OnUnconsciousPersonEncounter(other, owner);
			if (ignoreStimulusFromNowOn)
			{
				owner->TactileIgnore(stimSource);
			}
		}
		else
		{
			ignoreStimulusFromNowOn = false;
		}
	}
	else
	{
		// Not knocked out, not dead, deal with it
		if (owner->IsEnemy(other))
		{
			// Only do this if we don't have an enemy already
			if (owner->GetEnemy() == NULL)
			{
				// Living enemy
				gameLocal.Printf("I see a living enemy!\n");
				owner->SetEnemy(other);
				owner->AI_VISALERT = true;
				
				owner->SetAlertLevel(owner->thresh_5*2);
				memory.alertClass = EAlertVisual;
				memory.alertType = EAlertTypeEnemy;
				// An enemy should not be ignored in the future
				ignoreStimulusFromNowOn = false;
			}
		}
		else if (owner->IsFriend(other))
		{
			// Remember last time a friendly AI was seen
			memory.lastTimeFriendlyAISeen = gameLocal.time;

			if (!other->IsType(idAI::Type)) return; // safeguard
			idAI* otherAI = static_cast<idAI*>(other);

			Memory& otherMemory = otherAI->GetMemory();

			if (otherAI->AI_AlertLevel >= otherAI->thresh_3 && owner->AI_AlertLevel < owner->thresh_3)
			{
				// angua: the other AI is searching due to an alert, join in

				float otherAlertLevel = otherAI->AI_AlertLevel * 0.7f;
				
				if (otherAlertLevel > owner->AI_AlertLevel)
				{
					owner->SetAlertLevel(otherAlertLevel);

					owner->StopMove(MOVE_STATUS_DONE);
					
					memory.alertPos = otherMemory.alertPos;
					memory.alertClass = EAlertNone;
					memory.alertType = EAlertTypeSuspicious;
					
					memory.alertRadius = otherMemory.alertRadius;
					memory.alertSearchVolume = otherMemory.alertSearchVolume; 
					memory.alertSearchExclusionVolume.Zero();

					memory.alertedDueToCommunication = true;
				}
			}
			else
			{
				// Issue a communication stim to the friend we spotted.
				// We can issue warnings, greetings, etc...

				// Variables for the sound and the conveyed message
				idStr soundName;
				CommMessagePtr message; 
				
				if (memory.enemiesHaveBeenSeen)
				{
					if (otherAI != NULL && !otherAI->GetMind()->GetMemory().enemiesHaveBeenSeen)
					{
						gameLocal.Printf("I see a friend, I'm going to warn them that enemies have been seen.\n");
						message = CommMessagePtr(new CommMessage(
							CommMessage::ConveyWarning_EnemiesHaveBeenSeen_CommType, 
							owner, other, // from this AI to the other
							NULL,
							owner->GetPhysics()->GetOrigin()
						));
						soundName = "snd_warnSawEnemy";
					}
				}
				else if (memory.itemsHaveBeenStolen)
				{
					if (otherAI != NULL && !otherAI->GetMind()->GetMemory().itemsHaveBeenStolen)
					{
						gameLocal.Printf("I see a friend, I'm going to warn them that items have been stolen.\n");
						message = CommMessagePtr(new CommMessage(
							CommMessage::ConveyWarning_ItemsHaveBeenStolen_CommType, 
							owner, other, // from this AI to the other
							NULL,
							owner->GetPhysics()->GetOrigin()
						));
						soundName = "snd_warnMissingItem";
					}
				}
				else if (memory.countEvidenceOfIntruders >= MIN_EVIDENCE_OF_INTRUDERS_TO_COMMUNICATE_SUSPICION)
				{
					if (otherAI != NULL && otherAI->GetMind()->GetMemory().countEvidenceOfIntruders < memory.countEvidenceOfIntruders)
					{
						gameLocal.Printf("I see a friend, I'm going to warn them of evidence I'm concerned about\n");
						message = CommMessagePtr(new CommMessage(
							CommMessage::ConveyWarning_EvidenceOfIntruders_CommType, 
							owner, other, // from this AI to the other
							NULL,
							owner->GetPhysics()->GetOrigin()
						));
						soundName = "snd_warnSawEvidence";
					}
				}
				else if (owner->AI_AlertIndex < EObservant && owner->greetingState != ECannotGreet)
				{
					const idVec3& origin = owner->GetPhysics()->GetOrigin();
					const idVec3& otherOrigin = otherAI->GetPhysics()->GetOrigin();
					idVec3 dir = origin - otherOrigin;
					dir.z = 0;
					float distSqr = dir.LengthSqr();

					if (distSqr <= Square(230))
					{
						Memory::GreetingInfo& info = owner->GetMemory().GetGreetingInfo(otherAI);

						bool consider = true;

						// Owner has a certain chance of greeting when encountering the other person 
						// this chance does not get re-evaluated for a given amount of time
						// Basically this has the effect that AI evaluate the greeting chance
						// immediately after they enter the greeting radius and ignore all stims for 
						// the next 20 secs. (i.e. one chance evaluation per 20 seconds)
						if (info.lastConsiderTime > -1 && 
							gameLocal.time < info.lastConsiderTime + MIN_TIME_BETWEEN_GREETING_CHECKS)
						{
							// Not enough time has passed, ignore this stim
							consider = false;
						}
						else
						{
							info.lastConsiderTime = gameLocal.time;

							consider = (gameLocal.random.RandomFloat() > 1.0f - CHANCE_FOR_GREETING);
						}

						if (consider && 
							owner->CheckFOV(otherAI->GetEyePosition()) && 
							otherAI->CheckFOV(owner->GetEyePosition()))
						{
							// Clear the sound, a special GreetingBarkTask is handling this
							soundName = "";

							// Get the sound and queue the task
							idStr greetSound = GetGreetingSound(owner, otherAI);

							owner->commSubsystem->AddCommTask(
								CommunicationTaskPtr(new GreetingBarkTask(greetSound, otherAI))
							);
						}
					}
				}

				// Speak the chosen sound
				if (!soundName.IsEmpty() && gameLocal.time - memory.lastTimeVisualStimBark >= MINIMUM_SECONDS_BETWEEN_STIMULUS_BARKS)
				{
					memory.lastTimeVisualStimBark = gameLocal.time;
					owner->commSubsystem->AddCommTask(
						CommunicationTaskPtr(new SingleBarkTask(soundName, message))
					);
				}
			}

			// Don't ignore in future
			ignoreStimulusFromNowOn = false;
		}
		else
		{
			// Living neutral persons are not being handled, ignore it from now on
			ignoreStimulusFromNowOn = true;
		}
	}

	if (ignoreStimulusFromNowOn)
	{
		// We've seen this object, don't respond to it again
		stimSource->ResponseIgnore(ST_VISUAL, owner);
	}
}

idStr State::GetGreetingSound(idAI* owner, idAI* otherAI)
{
	idStr soundName;

	// Get the types of the two persons
	idStr ownPersonType(owner->spawnArgs.GetString(PERSONTYPE_KEY));
	idStr otherPersonType(otherAI->spawnArgs.GetString(PERSONTYPE_KEY));

	// the other AI is a priest
	if (otherPersonType == PERSONTYPE_PRIEST)
	{
		if (ownPersonType != PERSONTYPE_PRIEST)
		{
			if (owner->spawnArgs.FindKey( "snd_greeting_cleric") != NULL)
			{
				soundName = "snd_greeting_cleric";
			}
		}

		// priests use generic greeting for other priests
		else
		{
			if (owner->spawnArgs.FindKey("snd_greeting_generic") != NULL)
			{
				soundName = "snd_greeting_generic";
			}
		}

	}

	// the other AI is a builder
	else if (otherPersonType == PERSONTYPE_BUILDER)
	{
		if (owner->spawnArgs.FindKey( "snd_greeting_builder") != NULL)
		{
			soundName = "snd_greeting_builder";
		}
	}

	// the other AI is a pagan
	else if (otherPersonType == PERSONTYPE_PAGAN)
	{
		if (owner->spawnArgs.FindKey( "snd_greeting_pagan") != NULL)
		{
			soundName = "snd_greeting_pagan";
		}
	}

	// this AI is a noble
	else if (ownPersonType == PERSONTYPE_NOBLE)
	{
		// nobles use generic greeting for other nobles
		if (otherPersonType == PERSONTYPE_NOBLE)
		{
			if (owner->spawnArgs.FindKey("snd_greeting_generic") != NULL)
			{
				soundName = "snd_greeting_generic";
			}
		}

		// the other AI is a guard
		else if (otherAI->spawnArgs.GetBool("is_civilian") == 0 &&
				(otherAI->GetNumMeleeWeapons() > 0 || otherAI->GetNumRangedWeapons() > 0))
		{
			if (owner->spawnArgs.FindKey( "snd_greeting_noble_to_guard") != NULL)
			{
				soundName = "snd_greeting_noble_to_guard";
			}
		}

		// the other AI is a civilian
		else
		{
			if (owner->spawnArgs.FindKey( "snd_greeting_noble_to_civilian") != NULL)
			{
				soundName = "snd_greeting_noble_to_civilian";
			}
		}
	}


	if (soundName.IsEmpty())
	{

		// this AI is a guard
		if (owner->spawnArgs.GetBool("is_civilian") == false &&
			(owner->GetNumMeleeWeapons() > 0 || owner->GetNumRangedWeapons() > 0))
		{
			// the other AI is a noble
			if (otherPersonType == PERSONTYPE_NOBLE)
			{
				idStr otherPersonGender = otherAI->spawnArgs.GetString(PERSONGENDER_KEY);
				if (otherPersonGender == PERSONGENDER_FEMALE)
				{
					if (owner->spawnArgs.FindKey( "snd_greeting_guard_to_noble_female") != NULL)
					{
						soundName = "snd_greeting_guard_to_noble_female";
					}
					else if (owner->spawnArgs.FindKey( "snd_greeting_noble_female") != NULL)
					{
						soundName = "snd_greeting_noble_female";
					}
				}
				else if (otherPersonGender == PERSONGENDER_MALE)
				{
					if (owner->spawnArgs.FindKey( "snd_greeting_guard_to_noble_male") != NULL)
					{
						soundName = "snd_greeting_guard_to_noble_male";
					}
					else if (owner->spawnArgs.FindKey( "snd_greeting_noble_male") != NULL)
					{
						soundName = "snd_greeting_noble_male";
					}
				}
			}

			// the other AI is a guard
			else if (otherAI->spawnArgs.GetBool("is_civilian") == 0 &&
				(otherAI->GetNumMeleeWeapons() > 0 || otherAI->GetNumRangedWeapons() > 0))
			{	
				if (owner->spawnArgs.FindKey( "snd_greeting_guard_to_guard") != NULL)
				{
					soundName = "snd_greeting_guard_to_guard";
				}
				else if (owner->spawnArgs.FindKey( "snd_greeting_guard") != NULL)
				{
					soundName = "snd_greeting_guard";
				}
			}

			// the other AI is a civilian
			else
			{
				
				// check for gender specific barks
				idStr otherPersonGender = otherAI->spawnArgs.GetString(PERSONGENDER_KEY);
				if (otherPersonGender == PERSONGENDER_FEMALE)
				{
					if (owner->spawnArgs.FindKey("snd_greeting_guard_to_female") != NULL)
					{
						soundName = "snd_greeting_guard_to_female";
					}
					else if (owner->spawnArgs.FindKey("snd_greeting_female") != NULL)
					{
						soundName = "snd_greeting_female";
					}
				}
				else if (otherPersonGender == PERSONGENDER_MALE)
				{
					if (owner->spawnArgs.FindKey("snd_greeting_guard_to_male") != NULL)
					{
						soundName = "snd_greeting_guard_to_male";
					}
					else if (owner->spawnArgs.FindKey("snd_greeting_male") != NULL)
					{
						soundName = "snd_greeting_male";
					}
				}

				// no gender specific barks, use generic greeting to civilian
				if (soundName.IsEmpty())
				{
					if (owner->spawnArgs.FindKey("snd_greeting_guard_to_civilian") != NULL)
					{
						soundName = "snd_greeting_guard_to_civilian";
					}
					else if (owner->spawnArgs.FindKey("snd_greeting_civilian") != NULL)
					{
						soundName = "snd_greeting_civilian";
					}
				}
				
			}
		}

		// this AI is a civilian
		else
		{
			// the other AI is a noble
			if (otherPersonType == PERSONTYPE_NOBLE)
			{
				idStr otherPersonGender = otherAI->spawnArgs.GetString(PERSONGENDER_KEY);
				if (otherPersonGender == PERSONGENDER_FEMALE)
				{
					if (owner->spawnArgs.FindKey( "snd_greeting_civilian_to_noble_female") != NULL)
					{
						soundName = "snd_greeting_civilian_to_noble_female";
					}
					else if (owner->spawnArgs.FindKey( "snd_greeting_noble_female") != NULL)
					{
						soundName = "snd_greeting_noble_female";
					}
				}
				else if (otherPersonGender == PERSONGENDER_MALE)
				{
					if (owner->spawnArgs.FindKey( "snd_greeting_civilian_to_noble_male") != NULL)
					{
						soundName = "snd_greeting_civilian_to_noble_male";
					}
					else if (owner->spawnArgs.FindKey( "snd_greeting_noble_male") != NULL)
					{
						soundName = "snd_greeting_noble_male";
					}
				}
			}

			// the other AI is a guard
			else if (otherAI->spawnArgs.GetBool("is_civilian") == false &&
				(otherAI->GetNumMeleeWeapons() > 0 || otherAI->GetNumRangedWeapons() > 0))
			{	
				if (owner->spawnArgs.FindKey("snd_greeting_civilian_to_guard") != NULL)
				{
					soundName = "snd_greeting_civilian_to_guard";
				}
				else if (owner->spawnArgs.FindKey( "snd_greeting_guard") != NULL)
				{
					soundName = "snd_greeting_guard";
				}
			}

			// the other AI is a civilian
			else
			{
				// check for gender specific barks
				idStr otherPersonGender = otherAI->spawnArgs.GetString(PERSONGENDER_KEY);
				if (otherPersonGender == PERSONGENDER_FEMALE)
				{
					if (owner->spawnArgs.FindKey("snd_greeting_civilian_to_female") != NULL)
					{
						soundName = "snd_greeting_civilian_to_female";
					}
					else if (owner->spawnArgs.FindKey("snd_greeting_female") != NULL)
					{
						soundName = "snd_greeting_female";
					}
				}
				else if (otherPersonGender == PERSONGENDER_MALE)
				{
					if (owner->spawnArgs.FindKey("snd_greeting_civilian_to_male") != NULL)
					{
						soundName = "snd_greeting_civilian_to_male";
					}
					else if (owner->spawnArgs.FindKey("snd_greeting_male") != NULL)
					{
						soundName = "snd_greeting_male";
					}
				}

				// no gender specific barks, use generic greeting to civilian
				if (soundName.IsEmpty())
				{
					if (owner->spawnArgs.FindKey("snd_greeting_civilian_to_civilian") != NULL)
					{
						soundName = "snd_greeting_civilian_to_civilian";
					}
					else if (owner->spawnArgs.FindKey("snd_greeting_civilian") != NULL)
					{
						soundName = "snd_greeting_civilian";
					}
				}
				
			}
		}
	}

	// no sound found yet, use generic one
	if (soundName.IsEmpty())
	{
		if (owner->spawnArgs.FindKey("snd_greeting_generic") != NULL)
		{
			soundName = "snd_greeting_generic";
		}
	}
	return soundName;
}

idStr State::GetGreetingResponseSound(idAI* owner, idAI* otherAI)
{
	// Check for rank spawnargs
	int ownerRank = owner->spawnArgs.GetInt("rank", "0");
	int otherRank = otherAI->spawnArgs.GetInt("rank", "0");

	if (ownerRank != 0 && otherRank != 0)
	{
		// Rank spawnargs valid, compare
		return (ownerRank < otherRank) ? "snd_response_positive_superior" : "snd_response_positive";
	}

	// Get the type of persons
	idStr ownPersonType(owner->spawnArgs.GetString(PERSONTYPE_KEY));
	idStr otherPersonType(otherAI->spawnArgs.GetString(PERSONTYPE_KEY));

	// Nobles, elites or priest, just use the generic ones for everybody.
	if (ownPersonType == PERSONTYPE_NOBLE || ownPersonType == PERSONTYPE_ELITE || 
		ownPersonType == PERSONTYPE_PRIEST)
	{
		// Owner is a "superior"
		return "snd_response_positive";
	}

	// Owner is not superior, check other type

	// For most characters (civilian or guard), any noble, elite or priest would be considered a superior.
	bool otherIsSuperior = (otherPersonType == PERSONTYPE_NOBLE || 
		otherPersonType == PERSONTYPE_ELITE || otherPersonType == PERSONTYPE_PRIEST);

	return (otherIsSuperior) ? "snd_response_positive_superior" : "snd_response_positive";
}

bool State::OnDeadPersonEncounter(idActor* person, idAI* owner)
{
	assert(person != NULL && owner != NULL); // must be fulfilled
	
	// Memory shortcut
	Memory& memory = owner->GetMemory();

	if (owner->IsEnemy(person))
	{
		// The dead person is your enemy, ignore from now on
		return true;
	}
	else 
	{
		// The dead person is neutral or friendly, this is suspicious
		gameLocal.Printf("I see dead people!\n");
		memory.deadPeopleHaveBeenFound = true;

		// We've seen this object, don't respond to it again
		person->ResponseIgnore(ST_VISUAL, owner);

		// Three more piece of evidence of something out of place: A dead body is a REALLY bad thing
		memory.countEvidenceOfIntruders += 3;
		
		// Determine what to say
		idStr soundName;
		idStr personGender = person->spawnArgs.GetString(PERSONGENDER_KEY);

		if (idStr(person->spawnArgs.GetString(PERSONTYPE_KEY)) == owner->spawnArgs.GetString(PERSONTYPE_KEY))
		{
			soundName = "snd_foundComradeBody";
		}
		else if (personGender == PERSONGENDER_FEMALE)
		{
			soundName = "snd_foundDeadFemale";
		}
		else
		{
			soundName = "snd_foundDeadMale";
		}

		// Speak a reaction
		if (gameLocal.time - memory.lastTimeVisualStimBark >= MINIMUM_SECONDS_BETWEEN_STIMULUS_BARKS)
		{
			memory.lastTimeVisualStimBark = gameLocal.time;
			owner->commSubsystem->AddCommTask(
				CommunicationTaskPtr(new SingleBarkTask(soundName))
			);
		}

		// Raise alert level
		if (owner->AI_AlertLevel < owner->thresh_5 + 0.1f)
		{
			memory.alertPos = person->GetPhysics()->GetOrigin();
			memory.alertClass = EAlertVisual;
			memory.alertType = EAlertTypeDeadPerson;
			
			// Do search as if there is an enemy that has escaped
			memory.alertRadius = LOST_ENEMY_ALERT_RADIUS;
			memory.alertSearchVolume = LOST_ENEMY_SEARCH_VOLUME; 
			memory.alertSearchExclusionVolume.Zero();
			
			owner->AI_VISALERT = false;
			
			owner->SetAlertLevel(owner->thresh_5 + 0.1);
		}
					
		// Do new reaction to stimulus
		memory.investigateStimulusLocationClosely = true; // deep investigation
		memory.stimulusLocationItselfShouldBeSearched = true;
		memory.alertedDueToCommunication = false;
		
		// Callback for objectives
		owner->FoundBody(person);
	}

	// Ignore from now on
	return true;
}

bool State::OnUnconsciousPersonEncounter(idActor* person, idAI* owner)
{
	assert(person != NULL && owner != NULL); // must be fulfilled

	// Memory shortcut
	Memory& memory = owner->GetMemory();

	gameLocal.Printf("I see unconscious people!\n");

	if (owner->IsEnemy(person))
	{
		// The unconscious person is your enemy, ignore from now on
		return true;
	}
	else 
	{
		memory.unconsciousPeopleHaveBeenFound = true;

		// We've seen this object, don't respond to it again
		person->ResponseIgnore(ST_VISUAL, owner);

		// Determine what to say
		idStr soundName;
		idStr personGender = person->spawnArgs.GetString(PERSONGENDER_KEY);

		if (personGender == PERSONGENDER_FEMALE)
		{
			soundName = "snd_foundUnconsciousFemale";
		}
		else
		{
			soundName = "snd_foundUnconsciousMale";
		}

		// Speak a reaction
		memory.lastTimeVisualStimBark = gameLocal.time;
		owner->commSubsystem->AddCommTask(
			CommunicationTaskPtr(new SingleBarkTask(soundName))
		);

		// Raise alert level
		if (owner->AI_AlertLevel < owner->thresh_5 + 0.1f)
		{
			memory.alertPos = person->GetPhysics()->GetOrigin();
			memory.alertClass = EAlertVisual;
			memory.alertType = EAlertTypeUnconsciousPerson;
			
			// Do search as if there is an enemy that has escaped
			memory.alertRadius = LOST_ENEMY_ALERT_RADIUS;
			memory.alertSearchVolume = LOST_ENEMY_SEARCH_VOLUME; 
			memory.alertSearchExclusionVolume.Zero();
			
			owner->AI_VISALERT = false;
			
			owner->SetAlertLevel(owner->thresh_5 + 0.1f);
		}
					
		// Do new reaction to stimulus
		memory.investigateStimulusLocationClosely = true; // deep investigation
		memory.stimulusLocationItselfShouldBeSearched = true;
		memory.alertedDueToCommunication = false;
		
		// Callback for objectives
		owner->FoundBody(person);
	}

	// Ignore from now on
	return true;
}

void State::OnFailedKnockoutBlow(idEntity* attacker, const idVec3& direction, bool hitHead)
{
	idAI* owner = _owner.GetEntity();

	if (owner == NULL) return;

	// Switch to failed knockout state
	owner->GetMind()->PushState(StatePtr(new FailedKnockoutState(attacker, direction, hitHead)));
}

void State::OnProjectileHit(idProjectile* projectile, idEntity* attacker, int damageTaken)
{
	if (damageTaken > 0)
	{
		// Only consider the "no damage" case, as Pain() is already taking care of alerting us
		return;
	}

	idAI* owner = _owner.GetEntity();
	if (owner == NULL) return;

	DM_LOG(LC_AI, LT_INFO)LOGSTRING("AI %s has been hit by non-damaging projectile.\r", owner->name.c_str());

	if (!ShouldProcessAlert(EAlertTypeWeapon))
	{
		DM_LOG(LC_AI, LT_INFO)LOGSTRING("Ignoring non-damaging projectile hit.\r");
		return;
	}

	DM_LOG(LC_AI, LT_INFO)LOGSTRING("Alerting AI %s due to non-damaging projectile.\r", owner->name.c_str());
	
	if (owner->AI_AlertLevel < owner->thresh_5 - 0.1f)
	{
		Memory& memory = owner->GetMemory();

		// greebo: Set the alert position not directly to the attacker's origin, but let the AI 
		// search in the right direction
		const idVec3& ownerOrigin = owner->GetPhysics()->GetOrigin();

		idVec3 attackerDir(0,0,0);
		float distance = 0;
		
		if (attacker != NULL)
		{
			attackerDir = attacker->GetPhysics()->GetOrigin() - ownerOrigin;
			distance = attackerDir.NormalizeFast();
		}

		// Start searching halfways between us the attacker
		memory.alertPos = owner->GetPhysics()->GetOrigin() + attackerDir * distance * 0.5f;
		memory.alertClass = EAlertTactile;
		memory.alertType = EAlertTypeWeapon;
		
		// Do search as if there is an enemy that has escaped
		memory.alertRadius = TACTILE_ALERT_RADIUS;
		memory.alertSearchVolume = TACTILE_SEARCH_VOLUME*2; 
		memory.alertSearchExclusionVolume.Zero();

		memory.investigateStimulusLocationClosely = false;
		
		owner->AI_VISALERT = false;
		
		owner->SetAlertLevel(owner->thresh_5 - 0.1f);
	}
}

void State::OnMovementBlocked(idAI* owner)
{
	// Determine which type of object is blocking us

	const idVec3& ownerOrigin = owner->GetPhysics()->GetOrigin();

	// Set all attachments to nonsolid, temporarily
	owner->SaveAttachmentContents();
	owner->SetAttachmentContents(0);

	trace_t result;
	gameLocal.clip.TraceBounds(result, ownerOrigin, ownerOrigin + owner->viewAxis.ToAngles().ToForward()*20, owner->GetPhysics()->GetBounds(),
								CONTENTS_SOLID|CONTENTS_CORPSE, owner);

	owner->RestoreAttachmentContents();

	if (result.fraction >= 1.0f)
	{
		// Trace didn't hit anything?
		DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("AI %s is blocked but no entity in view\r", owner->name.c_str());
		return;
	}

	idEntity* ent = gameLocal.entities[result.c.entityNum];

	if (ent == NULL) 
	{
		// Trace didn't hit anything?
		DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("AI %s is blocked by NULL entity!\r", owner->name.c_str());
		return;
	}

	if (ent == gameLocal.world)
	{
		DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("AI %s is blocked by world!\r", owner->name.c_str());
		return;
	}

	DM_LOG(LC_AI, LT_DEBUG)LOGSTRING("AI %s is blocked by entity %s\r", owner->name.c_str(), ent->name.c_str());

	if (cv_ai_debug_blocked.GetBool())
	{
		gameRenderWorld->DebugBounds(colorRed, ent->GetPhysics()->GetBounds(), ent->GetPhysics()->GetOrigin(), 2000);
	}

	// Do something against this blocking entity, depending on its type
	if (ent->IsType(idAFAttachment::Type))
	{
		// This ought to be an AI's head, get its body
		ent = static_cast<idAFAttachment*>(ent)->GetBody();
	}

	if (ent->IsType(idAI::Type))
	{
		// Blocked by other AI
		idAI* master = owner;
		idAI* slave = static_cast<idAI*>(ent);

		if (!master->AI_FORWARD && slave->AI_FORWARD)
		{
			// Master is not moving, swap
			std::swap(master, slave);
		}

		if (slave->AI_FORWARD && slave->AI_RUN && master->AI_FORWARD && !master->AI_RUN)
		{
			// One AI is running, this should be the master
			std::swap(master, slave);
		}

		// Tell the slave to get out of the way, but only if none of the AI is currently resolving a block
		if (!slave->movementSubsystem->IsResolvingBlock() && !master->movementSubsystem->IsResolvingBlock())
		{
			slave->movementSubsystem->ResolveBlock(master);
		}
	}
	else if (ent->IsType(idStaticEntity::Type))
	{
		// Blocked by func_static, these are not considered by Obstacle Avoidance code.
		if (!owner->movementSubsystem->IsResolvingBlock())
		{
			owner->movementSubsystem->ResolveBlock(ent);
		}
	}
}

void State::OnVisualStimBlood(idEntity* stimSource, idAI* owner)
{
	assert(stimSource != NULL && owner != NULL); // must be fulfilled

	Memory& memory = owner->GetMemory();

	// Ignore from now on
	stimSource->ResponseIgnore(ST_VISUAL, owner);

	// angua: ignore blood after dead bodies have been found
	if (memory.deadPeopleHaveBeenFound)
	{
		return;
	}

	// Vocalize that see something out of place
	memory.lastTimeVisualStimBark = gameLocal.time;
	owner->commSubsystem->AddCommTask(
		CommunicationTaskPtr(new SingleBarkTask("snd_foundBlood"))
	);
	gameLocal.Printf("Is that blood!\n");
	
	// One more piece of evidence of something out of place
	memory.countEvidenceOfIntruders++;

	// Raise alert level
	if (owner->AI_AlertLevel < owner->thresh_5 - 0.1f)
	{
		memory.alertPos = stimSource->GetPhysics()->GetOrigin();
		memory.alertClass = EAlertVisual;
		memory.alertType = EAlertTypeBlood;

		
		// Do search as if there is an enemy that has escaped
		memory.alertRadius = LOST_ENEMY_ALERT_RADIUS;
		memory.alertSearchVolume = LOST_ENEMY_SEARCH_VOLUME; 
		memory.alertSearchExclusionVolume.Zero();
		
		owner->AI_VISALERT = false;

		owner->SetAlertLevel(owner->thresh_5 - 0.1f);
	}
				
	// Do new reaction to stimulus
	memory.investigateStimulusLocationClosely = true; // deep investigation
	memory.stimulusLocationItselfShouldBeSearched = true;
	memory.alertedDueToCommunication = false;
}

void State::OnVisualStimLightSource(idEntity* stimSource, idAI* owner)
{
	assert(stimSource != NULL && owner != NULL); // must be fulfilled

	Memory& memory = owner->GetMemory();

	idLight* light = dynamic_cast<idLight*>(stimSource);

	if (light == NULL)
	{
		// not a light
		return;
	}

	// Is it on?
	if (light->GetLightLevel() > 0)
	{
		// We've seen this light and it is on.
		// Don't respond to it again until it changes state and clears
		// its ignore list
		stimSource->ResponseIgnore(ST_VISUAL, owner);
		return;
	}

	// What type of light is it?
	idStr lightType = stimSource->spawnArgs.GetString(AIUSE_LIGHTTYPE_KEY);
	bool turnLightOn = false;

	// Is it supposed to be on?
	if (stimSource->spawnArgs.GetBool(AIUSE_SHOULDBEON_KEY))
	{
		// Vocalize that see something out of place because this light is supposed to be on
		gameLocal.Printf("Hey who turned of the light %s?\n", stimSource->name.c_str());
/*
		// Vocalize that see something out of place
		if (gameLocal.time - memory.lastTimeVisualStimBark >= MINIMUM_SECONDS_BETWEEN_STIMULUS_BARKS)
		{
			idStr soundName((lightType == AIUSE_LIGHTTYPE_TORCH) ? "snd_foundTorchOut" : "snd_foundLightsOff");

			memory.lastTimeVisualStimBark = gameLocal.time;
			owner->GetSubsystem(SubsysCommunication)->PushTask(
				TaskPtr(new SingleBarkTask(soundName))
			);
		}
*/
		// One more piece of evidence of something out of place
		memory.countEvidenceOfIntruders++;

		// Set up search
		if (owner->AI_AlertLevel < owner->thresh_4)
		{
			memory.alertPos = stimSource->GetPhysics()->GetOrigin();
			memory.alertClass = EAlertVisual;
			memory.alertType = EAlertTypeLightSource;
			
			// Prepare search as if there is an enemy that has escaped
			memory.alertRadius = LOST_ENEMY_ALERT_RADIUS;
			memory.alertSearchVolume = LOST_ENEMY_SEARCH_VOLUME; 
			memory.alertSearchExclusionVolume.Zero();
				
			owner->AI_VISALERT = false;
		}

		// Raise alert level if we already have some evidence of intruders
		if (owner->AI_AlertLevel < owner->thresh_3 && 
			(memory.enemiesHaveBeenSeen 
				|| memory.countEvidenceOfIntruders >= MIN_EVIDENCE_OF_INTRUDERS_TO_SEARCH_ON_LIGHT_OFF))
		{
			owner->SetAlertLevel(owner->thresh_3 + (owner->thresh_4 - owner->thresh_3) * 0.2
				* (memory.countEvidenceOfIntruders - MIN_EVIDENCE_OF_INTRUDERS_TO_SEARCH_ON_LIGHT_OFF));

			if (owner->AI_AlertLevel >= (owner->thresh_5 + owner->thresh_4) * 0.5)
			{
				owner->SetAlertLevel((owner->thresh_5 + owner->thresh_4) * 0.45);
			}
		}

		// Do new reaction to stimulus after relighting
		memory.stimulusLocationItselfShouldBeSearched = true;
		memory.alertedDueToCommunication = false;
	
		// We will be turning the light on
		turnLightOn = true;
	}

	// Check abilities to turn lights on
	if (lightType == AIUSE_LIGHTTYPE_TORCH && !owner->spawnArgs.GetBool("canLightTorches"))
	{
		// Can't light torches
		turnLightOn = false;
		stimSource->ResponseIgnore(ST_VISUAL, owner);
		if (gameLocal.time - memory.lastTimeVisualStimBark >= MINIMUM_SECONDS_BETWEEN_STIMULUS_BARKS)
		{
			memory.lastTimeVisualStimBark = gameLocal.time;
			owner->commSubsystem->AddCommTask(
				CommunicationTaskPtr(new SingleBarkTask("snd_noRelightTorch"))
			);
		}

	}
	else if (!owner->spawnArgs.GetBool("canOperateSwitchLights"))
	{
		// Can't operate switchlights
		turnLightOn = false;
		stimSource->ResponseIgnore(ST_VISUAL, owner);
	}
	else if (owner->HasSeenEvidence() || memory.countEvidenceOfIntruders >= MIN_EVIDENCE_OF_INTRUDERS_TO_TURN_ON_ALL_LIGHTS)
	{
		//gameLocal.Printf("For my safety, I should turn on the light %s\n", stimSource->name.c_str());
		turnLightOn = true;
	}

	// Turning the light on?
	if (turnLightOn && owner->AI_AlertLevel < (owner->thresh_5 + owner->thresh_4) * 0.5)
	{
		owner->GetMind()->SwitchState(StatePtr(new SwitchOnLightState(light)));
	}
}

void State::OnVisualStimMissingItem(idEntity* stimSource, idAI* owner)
{
	assert(stimSource != NULL && owner != NULL); // must be fulfilled

	Memory& memory = owner->GetMemory();

	// We've seen this object, don't respond to it again
	stimSource->ResponseIgnore(ST_VISUAL, owner);
	
	// Can we notice missing items
	if (owner->spawnArgs.GetFloat("chanceNoticeMissingItem") <= 0.0)
	{
		return;
	}

		// Does it belong to a friendly team
	if (stimSource->team != -1 && !owner->IsFriend(stimSource))
	{
		// Its not something we know about
		gameLocal.Printf("The missing item wasn't on my team\n");
		return;
	}

	float alert = owner->thresh_4 - 0.1f;

	if (stimSource->IsType(CAbsenceMarker::Type))
	{
		CAbsenceMarker* absenceMarker = static_cast<CAbsenceMarker*>(stimSource);
		const idDict& refSpawnargs = absenceMarker->GetRefSpawnargs();

		float chance(gameLocal.random.RandomFloat());
		if (chance >= refSpawnargs.GetFloat("absence_noticeability", "1"))
		{
			float recheckInterval = SEC2MS(refSpawnargs.GetFloat("absence_noticeability_recheck_interval", "60"));
			if (recheckInterval > 0.0f)
			{
				stimSource->PostEventMS( &EV_ResponseAllow, recheckInterval, ST_VISUAL, owner);
			}
			return;
		}
		if (refSpawnargs.GetFloat("absence_alert", "0") > 0)
		{
			alert = owner->AI_AlertLevel + refSpawnargs.GetFloat("absence_alert", "0");
		}
	}

	gameLocal.Printf("Something is missing from over there!\n");

	// Speak a reaction
	memory.lastTimeVisualStimBark = gameLocal.time;
	owner->commSubsystem->AddCommTask(
		CommunicationTaskPtr(new SingleBarkTask("snd_foundMissingItem"))
	);

	// One more piece of evidence of something out of place
	memory.itemsHaveBeenStolen = true;
	memory.countEvidenceOfIntruders++;

	// Raise alert level
	if (owner->AI_AlertLevel < alert)
	{
		memory.alertPos = stimSource->GetPhysics()->GetOrigin();
		memory.alertClass = EAlertVisual;
		memory.alertType = EAlertTypeMissingItem;

		
		// Prepare search as if there is an enemy that has escaped
		memory.alertRadius = LOST_ENEMY_ALERT_RADIUS;
		memory.alertSearchVolume = LOST_ENEMY_SEARCH_VOLUME; 
		memory.alertSearchExclusionVolume.Zero();
		
		owner->AI_VISALERT = false;
		
		owner->SetAlertLevel(alert);
	}
}


void State::OnVisualStimBrokenItem(idEntity* stimSource, idAI* owner)
{
	assert(stimSource != NULL && owner != NULL); // must be fulfilled

	Memory& memory = owner->GetMemory();

	// We've seen this object, don't respond to it again
	stimSource->ResponseIgnore(ST_VISUAL, owner);

	gameLocal.Printf("Something is broken over there!\n");

	owner->StopMove(MOVE_STATUS_DONE);
	owner->TurnToward(stimSource->GetPhysics()->GetOrigin());
	owner->Event_LookAtEntity(stimSource, 1);

	// Speak a reaction
	memory.lastTimeVisualStimBark = gameLocal.time;
	owner->commSubsystem->AddCommTask(
		CommunicationTaskPtr(new SingleBarkTask("snd_foundBrokenItem"))
	);

	owner->AI_RUN = true;

	// One more piece of evidence of something out of place
	memory.itemsHaveBeenBroken = true;
	memory.countEvidenceOfIntruders++;

	// Raise alert level
	if (owner->AI_AlertLevel < owner->thresh_4 - 0.1f)
	{
		memory.alertPos = stimSource->GetPhysics()->GetOrigin();
		memory.alertClass = EAlertVisual;
		memory.alertType = EAlertTypeBrokenItem;

		
		// Prepare search as if there is an enemy that has escaped
		memory.alertRadius = LOST_ENEMY_ALERT_RADIUS;
		memory.alertSearchVolume = LOST_ENEMY_SEARCH_VOLUME; 
		memory.alertSearchExclusionVolume.Zero();
		
		owner->AI_VISALERT = false;
		
		owner->SetAlertLevel(owner->thresh_5 - 0.1);
	}
}


void State::OnVisualStimDoor(idEntity* stimSource, idAI* owner)
{
	assert(stimSource != NULL && owner != NULL); // must be fulfilled

	Memory& memory = owner->GetMemory();
	CFrobDoor* door = static_cast<CFrobDoor*>(stimSource);

	// Update the info structure for this door
	DoorInfo& doorInfo = memory.GetDoorInfo(door);

	doorInfo.lastTimeSeen = gameLocal.time;
	doorInfo.wasOpen = door->IsOpen();

	// greebo: If the door is open, remove the corresponding area from the "forbidden" list
	if (door->IsOpen()) 
	{
		// Also, reset the "locked" property, open doors can't be locked
		doorInfo.wasLocked = false;

		// Enable the area for pathfinding again now that the door is open
		gameLocal.m_AreaManager.RemoveForbiddenArea(doorInfo.areaNum, owner);
	}

	// We've seen this object, don't respond to it again
	// Will get cleared if door changes state again
	stimSource->ResponseIgnore(ST_VISUAL, owner);

	// Is it supposed to be closed?
	if (!stimSource->spawnArgs.GetBool(AIUSE_SHOULDBECLOSED_KEY))
	{
		// door is not supposed to be closed, ignore
		return;
	}

	// Is it open?
	if (!door->IsOpen())
	{
		// ignore closed doors
		return;
	}

	// Vocalize that see something out of place
	memory.lastTimeVisualStimBark = gameLocal.time;
	owner->commSubsystem->AddCommTask(
		CommunicationTaskPtr(new SingleBarkTask("snd_foundOpenDoor"))
	);
	gameLocal.Printf("That door isn't supposed to be open!\n");
	
	// One more piece of evidence of something out of place
	memory.countEvidenceOfIntruders++;

	// Raise alert level
	if (owner->AI_AlertLevel < owner->thresh_4 - 0.1f)
	{
		memory.alertPos = stimSource->GetPhysics()->GetOrigin();
		memory.alertClass = EAlertVisual;
		memory.alertType = EAlertTypeDoor;
		
		// Do search as if there is an enemy that has escaped
		memory.alertRadius = LOST_ENEMY_ALERT_RADIUS;
		memory.alertSearchVolume = LOST_ENEMY_SEARCH_VOLUME; 
		memory.alertSearchExclusionVolume.Zero();
		
		owner->AI_VISALERT = false;

		owner->SetAlertLevel(owner->thresh_4 - 0.1f);
	}
				
	// Do new reaction to stimulus
	memory.stimulusLocationItselfShouldBeSearched = true;
	memory.alertedDueToCommunication = false;
}

void State::OnAICommMessage(CommMessage& message, float psychLoud)
{
	idAI* owner = _owner.GetEntity();
	// greebo: changed the IF back to an assertion, the owner should never be NULL
	assert(owner != NULL);

	if (owner->AI_DEAD || owner->AI_KNOCKEDOUT)
	{
		return;
	}

	// Get the message parameters
	CommMessage::TCommType commType = message.m_commType;
	
	idEntity* issuingEntity = message.m_p_issuingEntity.GetEntity();
	idEntity* recipientEntity = message.m_p_recipientEntity.GetEntity();
	idEntity* directObjectEntity = message.m_p_directObjectEntity.GetEntity();
	const idVec3& directObjectLocation = message.m_directObjectLocation;

	if (issuingEntity != NULL)
	{
		DM_LOG(LC_AI, LT_INFO)LOGSTRING("Got incoming message from %s\r", issuingEntity->name.c_str());
	}

	Memory& memory = owner->GetMemory();

	// greebo: Update the last AI seen timer on incoming messages from friendly AI
	if ((owner->GetPhysics()->GetOrigin() - directObjectLocation).LengthSqr() < Square(300) && 
		owner->IsFriend(issuingEntity))
	{
		memory.lastTimeFriendlyAISeen = gameLocal.time;
	}

	switch (commType)
	{
		case CommMessage::Greeting_CommType:
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Message Type: Greeting_CommType\r");
			// Have seen a friend
			memory.lastTimeFriendlyAISeen = gameLocal.time;

			// If not too upset, look at them
			if (owner->AI_AlertIndex < EObservant && owner->greetingState != ECannotGreet &&
				issuingEntity->IsType(idAI::Type))
			{
				idAI* otherAI = static_cast<idAI*>(issuingEntity);

				// Get the sound and queue the task
				idStr greetSound = GetGreetingResponseSound(owner, otherAI);

				owner->commSubsystem->AddCommTask(
					CommunicationTaskPtr(new GreetingBarkTask(greetSound, otherAI))
				);
			}
			break;
		case CommMessage::FriendlyJoke_CommType:
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Message Type: FriendlyJoke_CommType\r");
			// Have seen a friend
			memory.lastTimeFriendlyAISeen = gameLocal.time;

			if (directObjectEntity == owner)
			{
				gameLocal.Printf("Hah, yer no better!\n");
			}
			else
			{
				gameLocal.Printf("Ha, yer right, they be an ass\n");
			}
			break;
		case CommMessage::Insult_CommType:
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Message Type: Insult_CommType\r");
			if (directObjectEntity == owner)
			{
				gameLocal.Printf("Same to you, buddy\n");
			}
			else if (owner->IsEnemy(directObjectEntity))
			{
				gameLocal.Printf("Hah!\n");
			}
			else
			{
				gameLocal.Printf("I'm not gettin' involved\n");
			}
			break;
		case CommMessage::RequestForHelp_CommType:
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Message Type: RequestForHelp_CommType\r");
			if (owner->IsFriend(issuingEntity))
			{
				// Do we already have a target we are dealing with?
				if (owner->GetEnemy() != NULL)
				{
					gameLocal.Printf("I'm too busy, I have a target!\n");
					break;
				}

				if (directObjectEntity && directObjectEntity->IsType(idActor::Type))
				{
					
					// Bark
					//owner->GetSubsystem(SubsysCommunication)->PushTask(
					//	SingleBarkTaskPtr(new SingleBarkTask("snd_assistFriend"))
					//);

					gameLocal.Printf("Ok, I'm helping you.\n");

					owner->SetEnemy(static_cast<idActor*>(directObjectEntity));
					owner->GetMind()->PerformCombatCheck();
				}

				if (owner->GetEnemy() == NULL)
				{
					// no enemy set or enemy not found yet
					// set up search
					owner->AlertAI("aud", psychLoud);

					memory.alertPos = directObjectLocation;
					memory.alertRadius = LOST_ENEMY_ALERT_RADIUS;
					memory.alertSearchVolume = LOST_ENEMY_SEARCH_VOLUME;
					memory.alertSearchExclusionVolume.Zero();

					memory.alertedDueToCommunication = true;
					memory.stimulusLocationItselfShouldBeSearched = true;
				}
				
			}
			else if (owner->AI_AlertLevel < owner->thresh_1 + (owner->thresh_2 - owner->thresh_1) * 0.5f)
			{
				owner->SetAlertLevel(owner->thresh_1 + (owner->thresh_2 - owner->thresh_1) * 0.5f);
			}
			break;
		case CommMessage::RequestForMissileHelp_CommType:
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Message Type: RequestForMissileHelp_CommType\r");
			// Respond if they are a friend and we have a ranged weapon
			if (owner->IsFriend(issuingEntity) && owner->GetNumRangedWeapons() > 0)
			{
				// Do we already have a target we are dealing with?
				if (owner->GetEnemy() != NULL)
				{
					gameLocal.Printf("I'm too busy, I have a target!\n");
					break;
				}

				if (directObjectEntity->IsType(idActor::Type))
				{
					gameLocal.Printf("I'll attack it with my ranged weapon!\n");

					// Bark
					//owner->GetSubsystem(SubsysCommunication)->PushTask(
					//	SingleBarkTaskPtr(new SingleBarkTask("snd_assistFriend"))
					//);
					
					owner->SetEnemy(static_cast<idActor*>(directObjectEntity));
					owner->GetMind()->PerformCombatCheck();
				}
			}
			else 
			{
				//gameLocal.Printf("I don't have a ranged weapon or I am not getting involved.\n");
				if (owner->AI_AlertLevel < owner->thresh_2*0.5f)
				{
					owner->SetAlertLevel(owner->thresh_2*0.5f);
				}
			}
			break;
		case CommMessage::RequestForMeleeHelp_CommType:
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Message Type: RequestForMeleeHelp_CommType\r");
			// Respond if they are a friend and we have a melee weapon
			if (owner->IsFriend(issuingEntity) && owner->GetNumMeleeWeapons() > 0)
			{
				// Do we already have a target we are dealing with?
				if (owner->GetEnemy() != NULL)
				{
					gameLocal.Printf("I'm too busy, I have a target!\n");
					break;
				}

				if (directObjectEntity->IsType(idActor::Type))
				{
					gameLocal.Printf("I'll attack it with my melee weapon!\n");

					// Bark
					//owner->GetSubsystem(SubsysCommunication)->PushTask(
					//	SingleBarkTaskPtr(new SingleBarkTask("snd_assistFriend"))
					//);
					
					owner->SetEnemy(static_cast<idActor*>(directObjectEntity));
					owner->GetMind()->PerformCombatCheck();
				}
			}
			else 
			{
				gameLocal.Printf("I don't have a melee weapon or I am not getting involved.\n");
				if (owner->AI_AlertLevel < owner->thresh_2*0.5f)
				{
					owner->SetAlertLevel(owner->thresh_2*0.5f);
				}
			}
			break;
		case CommMessage::RequestForLight_CommType:
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Message Type: RequestForLight_CommType\r");
			gameLocal.Printf("I don't know how to bring light!\n");
			break;
		case CommMessage::DetectedSomethingSuspicious_CommType:
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Message Type: DetectedSomethingSuspicious_CommType\r");
			OnMessageDetectedSomethingSuspicious(message);
			break;
		case CommMessage::DetectedEnemy_CommType:
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Message Type: DetectedEnemy_CommType\r");
			//gameLocal.Printf("Somebody spotted an enemy... (%s)\n", directObjectEntity->name.c_str());
	
			if (owner->GetEnemy() != NULL)
			{
				//gameLocal.Printf("I'm too busy with my own target!\n");
				return;
			}

			{
				float newAlertLevel = (owner->thresh_4 + owner->thresh_5) * 0.5f;

				// greebo: Only set the alert level if it is greater than our own
				if (owner->AI_AlertLevel < newAlertLevel && 
					owner->IsFriend(issuingEntity) && 
					owner->IsEnemy(directObjectEntity))
				{
					// Set the alert level between 4 and 5.
					owner->SetAlertLevel((owner->thresh_4 + owner->thresh_5)*0.5f);
					
					// We got alerted by a communication message
					memory.alertedDueToCommunication = true;
						
					//gameLocal.Printf("They're my friend, I'll attack it too!\n");
					memory.alertPos = directObjectLocation;
				}
			}
			break;
		case CommMessage::FollowOrder_CommType:
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Message Type: FollowOrder_CommType\r");
			if (recipientEntity == owner && owner->IsFriend(issuingEntity))
			{
				gameLocal.Printf("But I don't know how to follow somebody!\n");
			}
			break;
		case CommMessage::GuardLocationOrder_CommType:
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Message Type: GuardLocationOrder_CommType\r");
			if (recipientEntity == owner && owner->IsFriend(issuingEntity))
			{
				gameLocal.Printf("But I don't know how to guard a location!\n");
			}
			break;
		case CommMessage::GuardEntityOrder_CommType:
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Message Type: GuardEntityOrder_CommType\r");
			if (recipientEntity == owner && owner->IsFriend(issuingEntity))
			{
				gameLocal.Printf("But I don't know how to guard an entity!\n");
			}
			break;
		case CommMessage::PatrolOrder_CommType:
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Message Type: PatrolOrder_CommType\r");
			if (recipientEntity == owner && owner->IsFriend(issuingEntity))
			{
				gameLocal.Printf("But I don't know how to switch my patrol route!\n");
			}
			break;
		case CommMessage::SearchOrder_CommType:
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Message Type: SearchOrder_CommType\r");
			if (recipientEntity == owner && owner->IsFriend(issuingEntity))
			{
				// Set alert pos to the position we were ordered to search
				memory.alertPos = directObjectLocation;
				memory.chosenHidingSpot = directObjectLocation;

				owner->SetAlertLevel((owner->thresh_3 + owner->thresh_4)*0.5f);
			}
			break;
		case CommMessage::AttackOrder_CommType:
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Message Type: AttackOrder_CommType\r");
			// Set this as our enemy and enter combat
			if (recipientEntity == owner && owner->IsFriend(issuingEntity))
			{
				gameLocal.Printf("Yes sir! Attacking your specified target!\n");

				if (directObjectEntity->IsType(idActor::Type))
				{
					owner->SetEnemy(static_cast<idActor*>(directObjectEntity));
					owner->SetAlertLevel(owner->thresh_5*2);
				}
			}
			else if (owner->AI_AlertLevel < owner->thresh_2*0.5f)
			{
				owner->SetAlertLevel(owner->thresh_2*0.5f);
			}
			break;
		case CommMessage::GetOutOfTheWayOrder_CommType:
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Message Type: GetOutOfTheWayOrder_CommType\r");
			break;
		case CommMessage::ConveyWarning_EvidenceOfIntruders_CommType:
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Message Type: ConveyWarning_EvidenceOfIntruders_CommType\r");
			if (issuingEntity->IsType(idAI::Type))
			{
				idAI* issuer = static_cast<idAI*>(issuingEntity);
				// Note: We deliberately don't care if the issuer is a friend or not
				int warningAmount = issuer->GetMind()->GetMemory().countEvidenceOfIntruders;
				
				if (memory.countEvidenceOfIntruders < warningAmount)
				{
					gameLocal.Printf("I've been warned about evidence of intruders.\n");
					memory.countEvidenceOfIntruders = warningAmount;

					if (owner->AI_AlertLevel < owner->thresh_2*0.5f)
					{
						owner->SetAlertLevel(owner->thresh_2*0.5f);
					}
				}
			}
			break;
		case CommMessage::ConveyWarning_ItemsHaveBeenStolen_CommType:
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Message Type: ConveyWarning_ItemsHaveBeenStolen_CommType\r");
			// Note: We deliberately don't care if the issuer is a friend or not
			if (!memory.itemsHaveBeenStolen)
			{
				gameLocal.Printf("I've been warned that items have been stolen.\n");
				memory.itemsHaveBeenStolen = true;

				if (owner->AI_AlertLevel < owner->thresh_2*0.5f)
				{
					owner->SetAlertLevel(owner->thresh_2*0.5f);
				}
			}
			break;
		case CommMessage::ConveyWarning_EnemiesHaveBeenSeen_CommType:
			DM_LOG(LC_AI, LT_INFO)LOGSTRING("Message Type: ConveyWarning_EnemiesHaveBeenSeen_CommType\r");
			// Note: We deliberately don't care if the issuer is a friend or not
			if (!memory.enemiesHaveBeenSeen)
			{
				gameLocal.Printf("I've been warned that enemies have been seen.\n");
				memory.enemiesHaveBeenSeen = true;
				
				if (owner->AI_AlertLevel < owner->thresh_2*0.5f)
				{
					owner->SetAlertLevel(owner->thresh_2*0.5f);
				}
			}
			break;
	} // switch
}

void State::OnMessageDetectedSomethingSuspicious(CommMessage& message)
{
	idEntity* issuingEntity = message.m_p_issuingEntity.GetEntity();
	idEntity* recipientEntity = message.m_p_recipientEntity.GetEntity();
	idEntity* directObjectEntity = message.m_p_directObjectEntity.GetEntity();
	idVec3 directObjectLocation = message.m_directObjectLocation;

	idAI* owner = _owner.GetEntity();
	assert(owner != NULL);

	if (owner->AI_DEAD || owner->AI_KNOCKEDOUT)
	{
		return;
	}

	Memory& memory = owner->GetMemory();

	//gameLocal.Printf("Somebody else noticed something suspicious...\n");

	if (owner->GetEnemy() != NULL)
	{
		//gameLocal.Printf ("I'm too busy with my own target!");
		return;
	}

	if (owner->IsFriend(issuingEntity))
	{
		// If not already searching something else
		if (owner->AI_AlertLevel >= owner->thresh_3)
		{
			//gameLocal.Printf ("I'm too busy searching something else\n");
			return;
		}
		
		//gameLocal.Printf ("They're my friend, I'll look too!\n");
		
		// Get some search points from them.
		int numSpots = owner->GetSomeOfOtherEntitiesHidingSpotList(issuingEntity);

		if (numSpots > 0)
		{
			// What is the distance to the friend.  If it is greater than a certain amount, shout intention
			// to come help
			float distanceToIssuer = (issuingEntity->GetPhysics()->GetOrigin() - owner->GetPhysics()->GetOrigin()).LengthFast();
			if (distanceToIssuer >= MIN_DISTANCE_TO_ISSUER_TO_SHOUT_COMING_TO_ASSISTANCE)
			{
				// Bark
				// angua: this one was getting quite annoying if you hear it all the time
				//owner->GetSubsystem(SubsysCommunication)->PushTask(
				//	SingleBarkTaskPtr(new SingleBarkTask("snd_assistFriend"))
				//);
			}
			
			// If AI that called out has a higher alert num, raise ours
			// to match theres due to urgency in their voice
			float otherAlertLevel = 0.0f;
			
			if (issuingEntity->IsType(idAI::Type))
			{
				// Inherit the alert level of the other AI, but attenuate it a bit
				otherAlertLevel = static_cast<idAI*>(issuingEntity)->AI_AlertLevel * 0.7f;
			}

			//gameLocal.Printf("The AI who noticed something has an alert num of %f\n", otherAlertLevel);
			if (otherAlertLevel > owner->AI_AlertLevel)
			{
				owner->SetAlertLevel(otherAlertLevel);
				owner->StopMove(MOVE_STATUS_DONE);

				Memory& otherMemory = static_cast<idAI*>(issuingEntity)->GetMemory();

				memory.alertPos = otherMemory.alertPos;
				memory.alertClass = EAlertNone;
				memory.alertType = EAlertTypeSuspicious;
				
				memory.alertRadius = otherMemory.alertRadius;
				memory.alertSearchVolume = otherMemory.alertSearchVolume; 
				memory.alertSearchExclusionVolume.Zero();


				memory.alertedDueToCommunication = true;
			}
			
			return;
		}
		else
		{
			//gameLocal.Printf("Hmpfh, no spots to help them with :(\n");
		}
		
	}
	else if (owner->AI_AlertLevel < owner->thresh_2*0.5f)
	{
		owner->SetAlertLevel(owner->thresh_2*0.5f);
	}
}

void State::OnFrobDoorEncounter(CFrobDoor* frobDoor)
{
	idAI* owner = _owner.GetEntity();
	assert(owner != NULL);

	if (cv_ai_door_show.GetBool()) 
	{
		gameRenderWorld->DebugArrow(colorRed, owner->GetEyePosition(), frobDoor->GetPhysics()->GetOrigin(), 1, 16);
	}

	Memory& memory = owner->GetMemory();

	// check if we already have a door to handle
	// don't start a DoorHandleTask if it is the same door or the other part of a double door
	CFrobDoor* currentDoor = memory.doorRelated.currentDoor.GetEntity();
	if (currentDoor == NULL)
	{
		memory.doorRelated.currentDoor = frobDoor;
		owner->movementSubsystem->PushTask(HandleDoorTask::CreateInstance());
	}
	else 
	{
		if (frobDoor != currentDoor && frobDoor != currentDoor->GetDoubleDoor())
		{
			// this is a new door

			// if there is already a door handling task active, 
			// terminate that one so we can start a new one next time
			const SubsystemPtr& subsys = owner->movementSubsystem;
			TaskPtr task = subsys->GetCurrentTask();

			if (boost::dynamic_pointer_cast<HandleDoorTask>(task) != NULL)
			{
				subsys->FinishTask();
			}
			else
			{
				// angua: current door is set but no door handling task active
				// door handling task was probably terminated before inititalisation
				// clear current door
				memory.doorRelated.currentDoor = NULL;
			}
		}
		// this is our current door
		else
		{
			// this is already our current door

			const SubsystemPtr& subsys = owner->movementSubsystem;
			TaskPtr task = subsys->GetCurrentTask();

			if (boost::dynamic_pointer_cast<HandleDoorTask>(task) == NULL)
			{
				// angua: current door is set but no door handling task active
				// door handling task was probably terminated before inititalisation
				// clear current door
				memory.doorRelated.currentDoor = NULL;
			}
		}
	}
}

void State::NeedToUseElevator(const eas::RouteInfoPtr& routeInfo)
{
	idAI* owner = _owner.GetEntity();
	assert(owner != NULL);

	if (!owner->m_HandlingElevator && owner->CanUseElevators())
	{
		// Prevent more ElevatorTasks from being pushed
		owner->m_HandlingElevator = true;
		owner->movementSubsystem->PushTask(TaskPtr(new HandleElevatorTask(routeInfo)));
	}
}


} // namespace ai