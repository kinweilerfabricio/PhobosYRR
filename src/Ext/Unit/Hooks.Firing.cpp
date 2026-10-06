#include "Body.h"

#include <Ext/WeaponType/Body.h>
#include <cmath>
#include <DriveLocomotionClass.h>

DEFINE_JUMP(LJMP, 0x741406, 0x741427)

DEFINE_HOOK(0x736F61, UnitClass_UpdateFiring_FireUp, 0x6)
{
	enum { SkipFiring = 0x737063 };

	GET(UnitClass* const, pThis, ESI);
	GET(const int, weaponIndex, EDI);

	const auto pType = pThis->Type;

	if (pType->Turret || pType->Voxel || pThis->InLimbo)
		return 0;

	const auto pExt = UnitExt::Fetch(pThis);
	const auto pTypeExt = pExt->GetTypeExtData();

	// SHP vehicles have no secondary action frames, so it does not need SecondaryFire.
	const int fireUp = pTypeExt->FireUp;

	if (fireUp >= 0 && !pType->OpportunityFire && pThis->Locomotor->Is_Really_Moving_Now())
	{
		if (pThis->CurrentFiringFrame != -1)
			pThis->CurrentFiringFrame = -1;

		return SkipFiring;
	}

	const int firingFrames = pType->FiringFrames;
	const int frames = 2 * firingFrames - 1;

	auto const pWeapon = pThis->GetWeapon(weaponIndex)->WeaponType;

	if (frames >= 0)
	{
		bool updateFiringFrame = true;

		if (!pTypeExt->IsSecondary(weaponIndex))
		{
			const int value = pThis->CurrentBurstIndex % pWeapon->Burst;
			const int syncFrame = value >= 2 ? -1
				: (value == 0 ? pType->FiringSyncFrame0 : pType->FiringSyncFrame1);

			updateFiringFrame = syncFrame == -1;
		}

		if (pThis->CurrentFiringFrame == -1
			|| (fireUp < 0 && updateFiringFrame))
		{
			pThis->CurrentFiringFrame = frames;
		}
	}

	if (fireUp >= 0)
	{
		int cumulativeDelay = 0;
		int projectedDelay = 0;
		auto const pWeaponExt = WeaponTypeExt::Fetch(pWeapon);
		const bool allowBurst = pWeaponExt->Burst_FireWithinSequence;
		const int currentBurstIndex = pThis->CurrentBurstIndex;
		auto& random = ScenarioClass::Instance->Random;

		// Calculate cumulative burst delay as well cumulative delay after next shot (projected delay).
		if (allowBurst)
		{
			for (int i = 0; i <= currentBurstIndex; i++)
			{
				const int burstDelay = pWeaponExt->GetBurstDelay(i);
				int delay = (burstDelay > -1) ? burstDelay : random.RandomRanged(3, 5);

				// Other than initial delay, treat 0 frame delays as 1 frame delay due to per-frame processing.
				if (i != 0)
					delay = Math::max(delay, 1);

				cumulativeDelay += delay;

				if (i == currentBurstIndex)
					projectedDelay = cumulativeDelay + delay;
			}
		}

		const int frame = (frames - pThis->CurrentFiringFrame) / 2;
		const int firingFrame = fireUp + cumulativeDelay;

		if (TechnoExt::HandleDelayedFireWithPauseSequence(pThis, pWeapon, weaponIndex, frame, firingFrame))
			return SkipFiring;

		if (frame != firingFrame)
		{
			return SkipFiring;
		}
		else if (allowBurst)
		{
			// If projected frame for firing next shot goes beyond the sequence frame count, cease firing after this shot and start rearm timer.
			if (fireUp + projectedDelay > frames)
				pExt->ForceFullRearmDelay = true;
		}
	}
	else if (TechnoExt::HandleDelayedFireWithPauseSequence(pThis, pWeapon, weaponIndex, 0, -1))
	{
		return SkipFiring;
	}

	return 0;
}

DEFINE_HOOK(0x736F67, UnitClass_UpdateFiring_BurstNoDelay, 0x6)
{
	enum { SkipVanillaFire = 0x737063 };

	GET(UnitClass* const, pThis, ESI);
	GET(const int, weaponIndex, EDI);
	GET(AbstractClass* const, pTarget, EAX);

	if (const auto pWeapon = pThis->GetWeapon(weaponIndex)->WeaponType)
	{
		if (pWeapon->Burst > 1)
		{
			const auto pWeaponExt = WeaponTypeExt::Fetch(pWeapon);

			if (pWeaponExt->Burst_NoDelay && (!pWeaponExt->DelayedFire_Duration.isset() || pWeaponExt->DelayedFire_OnlyOnInitialBurst))
			{
				if (pThis->Fire(pTarget, weaponIndex))
				{
					if (!pThis->CurrentBurstIndex)
						return SkipVanillaFire;

					int rof = pThis->RearmTimer.TimeLeft;
					pThis->RearmTimer.Start(0);

					for (int i = pThis->CurrentBurstIndex; i < pWeapon->Burst && pThis->GetFireError(pTarget, weaponIndex, true) == FireError::OK && pThis->Fire(pTarget, weaponIndex); ++i)
					{
						rof = pThis->RearmTimer.TimeLeft;
						pThis->RearmTimer.Start(0);
					}

					pThis->RearmTimer.Start(rof);
				}

				return SkipVanillaFire;
			}
		}
	}

	return 0;
}

DEFINE_HOOK(0x741A96, UnitClass_SetDestination_ResetFiringFrame, 0x6)
{
	GET(UnitClass* const, pThis, EBP);

	if (!pThis->Target && !pThis->Type->Turret
		&& pThis->CurrentFiringFrame != -1)
	{
		pThis->CurrentFiringFrame = -1;
	}

	return 0;
}

/*DEFINE_HOOK(0x736DF0, UnitClass_UpdateFiring_ForceChassis, 0x5)
{
	GET(UnitClass* const, pThis, ECX);

	if (pThis->Target && pThis->IsAlive)
	{
		int weaponIdx = pThis->SelectWeapon(pThis->Target);

		if (weaponIdx == 0 && pThis->Target->WhatAmI() == AbstractType::Infantry)
		{
			weaponIdx = 1;
		}

		if (weaponIdx == 1)
		{
			WeaponStruct* pWeap = pThis->GetWeapon(weaponIdx);

			if (pWeap && pWeap->WeaponType)
			{
				auto pWeapExt = WeaponTypeExt::ExtMap.Find(pWeap->WeaponType);

				if (pWeapExt && pWeapExt->AlwaysFaceTarget.Get())
				{
					CoordStruct myC = pThis->GetCoords();
					CoordStruct tarC = pThis->Target->GetCoords();

					double dx = myC.X - tarC.X;
					double dy = myC.Y - tarC.Y;
					double dist = std::sqrt(dx * dx + dy * dy);

					double rangeInLeptons = pWeap->WeaponType->Range * 256.0;

					if (dist <= rangeInLeptons)
					{
						// 1. Guardamos la dirección perfecta hacia el objetivo (el motor ya la calculó en la torreta)
						DirStruct targetDir = pThis->SecondaryFacing.Desired();

						// 2. Le ordenamos al chasis que empiece a rotar hacia allá
						pThis->PrimaryFacing.SetDesired(targetDir);

						// 3. SIMULACIÓN DE "TURRET=NO": 
						// Pisamos toda la memoria de la torreta y la igualamos al chasis.
						// Queda trabada apuntando al frente del tanque de forma permanente mientras use este arma.
						pThis->SecondaryFacing = pThis->PrimaryFacing;

						// 4. Forzamos la actualización de la animación en este frame
						pThis->UpdateRotation();
					}
					else
					{
						// Inyección de ruta si se quedó trabado lejos
						if (Unsorted::CurrentFrame % 30 == 0)
						{
							pThis->AssignDestination_7447B0(pThis->Target);
						}
					}
				}
			}
		}
	}

	return 0;
}*/

/*DEFINE_HOOK(0x736DF0, UnitClass_UpdateFiring_NativeLock, 0x5)
{
	GET(UnitClass* const, pThis, ECX);

	// Verificamos que tenga objetivo, esté vivo y comprobamos el smart pointer
	if (pThis->Target && pThis->IsAlive && pThis->Locomotor != nullptr)
	{
		int weaponIdx = pThis->SelectWeapon(pThis->Target);

		if (weaponIdx == 0 && pThis->Target->WhatAmI() == AbstractType::Infantry)
		{
			weaponIdx = 1;
		}

		// 1. Extraemos el puntero crudo del Smart Pointer de COM
		ILocomotion* pRawLoco = pThis->Locomotor;

		// 2. Ahora sí podemos leer la VTable del puntero real sin errores de compilación
		if (pRawLoco && *(uintptr_t*)pRawLoco == DriveLocomotionClass::ILocoVTable)
		{
			// Casteamos con total seguridad
			DriveLocomotionClass* pLoco = (DriveLocomotionClass*)pRawLoco;

			if (weaponIdx == 1)
			{
				WeaponStruct* pWeap = pThis->GetWeapon(weaponIdx);

				if (pWeap && pWeap->WeaponType)
				{
					auto pWeapExt = WeaponTypeExt::ExtMap.Find(pWeap->WeaponType);

					if (pWeapExt && pWeapExt->AlwaysFaceTarget.Get())
					{
						// Trabamos la torreta para que el locomotor gire todo el chasis
						pLoco->IsTurretLockedDown = 1;
						pThis->PrimaryFacing.SetDesired(pThis->SecondaryFacing.Desired());
					}
					else
					{
						pLoco->IsTurretLockedDown = 0;
					}
				}
			}
			else
			{
				// Si usa el cañón principal (0), destrabamos la torreta
				pLoco->IsTurretLockedDown = 0;
			}
		}
	}

	return 0;
}
*/

DEFINE_HOOK(0x736DF0, UnitClass_UpdateFiring_RammingMove, 0x5)
{
	GET(UnitClass* const, pThis, ECX);

	if (pThis->Target && pThis->IsAlive)
	{
		int weaponIdx = pThis->SelectWeapon(pThis->Target);

		// Hack para priorizar el aplaste contra infantería
		if (weaponIdx == 1 && pThis->Target->WhatAmI() == AbstractType::Infantry)
		{
			weaponIdx = 0;
		}

		if (weaponIdx == 0)
		{
			WeaponStruct* pWeap = pThis->GetWeapon(weaponIdx);

			if (pWeap && pWeap->WeaponType)
			{
				auto pWeapExt = WeaponTypeExt::ExtMap.Find(pWeap->WeaponType);

				if (pWeapExt && pWeapExt->AlwaysFaceTarget.Get())
				{
					// TU IDEA: Lo obligamos a caminar hacia la víctima.
					// Al inyectarle el destino de forma constante, el locomotor del juego 
					// asume el control nativo, enciende las orugas y gira el chasis solo.

					// Lo llamamos cada 15 frames (cuarto de segundo) para que 
					// recalcule si el soldado corre, sin saturar la RAM.
					if (Unsorted::CurrentFrame % 15 == 0)
					{
						pThis->AssignDestination_7447B0(pThis->Target);
					}
				}
			}
		}
	}

	return 0; // El motor procesa el disparo normal mientras el tanque avanza
}
