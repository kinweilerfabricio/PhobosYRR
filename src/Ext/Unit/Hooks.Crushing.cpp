#include <DriveLocomotionClass.h>
#include <ShipLocomotionClass.h>

#include "Body.h"

DEFINE_HOOK(0x73B05B, UnitClass_PerCellProcess_TiltWhenCrushes, 0x6)
{
	enum { SkipGameCode = 0x73B074 };

	GET(UnitClass*, pThis, EBP);

	auto const pType = pThis->Type;
	auto const pTypeExt = UnitTypeExt::Fetch(pType);

	if (!pTypeExt->TiltsWhenCrushes_Overlays.Get(pType->TiltsWhenCrushes))
		return SkipGameCode;

	pThis->RockingForwardsPerFrame += static_cast<float>(pTypeExt->CrushOverlayExtraForwardTilt);

	return SkipGameCode;
}

DEFINE_HOOK(0x741941, UnitClass_OverrunSquare_TiltWhenCrushes, 0x6)
{
	enum { SkipGameCode = 0x74195E };

	GET(UnitClass*, pThis, EDI);

	auto const pType = pThis->Type;
	auto const pTypeExt = UnitTypeExt::Fetch(pType);

	if (!pTypeExt->TiltsWhenCrushes_Vehicles.Get(pType->TiltsWhenCrushes))
		return SkipGameCode;

	pThis->RockingForwardsPerFrame = static_cast<float>(pTypeExt->CrushForwardTiltPerFrame.Get(-0.050000001));

	return SkipGameCode;
}

DEFINE_HOOK(0x4B1150, DriveLocomotionClass_WhileMoving_CrushSlowdown, 0x9)
{
	enum { SkipGameCode = 0x4B116B };

	GET(DriveLocomotionClass*, pThis, EBP);

	auto const pTypeExt = static_cast<UnitExt*>(TechnoExt::Fetch(pThis->LinkedTo))->GetTypeExtData();
	auto slowdownCoefficient = pThis->movementspeed_50;
	const double mult = pTypeExt->CrushSlowdownMultiplier.Get(RulesExt::Global()->CrushSlowdownMultiplier);

	if (slowdownCoefficient > mult)
		slowdownCoefficient = mult;

	__asm { fld slowdownCoefficient };

	return SkipGameCode;

}

DEFINE_HOOK_AGAIN(0x4B1A4B, DriveLocomotionClass_WhileMoving_CrushTilt, 0xD)
DEFINE_HOOK(0x4B19F7, DriveLocomotionClass_WhileMoving_CrushTilt, 0xD)
{
	enum { SkipGameCode1 = 0x4B1A04, SkipGameCode2 = 0x4B1A58 };

	GET(DriveLocomotionClass*, pThis, EBP);

	auto const pLinkedTo = pThis->LinkedTo;
	auto const pTypeExt = static_cast<UnitExt*>(TechnoExt::Fetch(pLinkedTo))->GetTypeExtData();
	pLinkedTo->RockingForwardsPerFrame = static_cast<float>(pTypeExt->CrushForwardTiltPerFrame.Get(-0.050000001));

	return R->Origin() == 0x4B19F7 ? SkipGameCode1 : SkipGameCode2;
}

DEFINE_HOOK(0x6A0813, ShipLocomotionClass_WhileMoving_CrushSlowdown, 0xB)
{
	enum { SkipGameCode = 0x6A082E };

	GET(ShipLocomotionClass*, pThis, EBP);

	auto const pTypeExt = static_cast<UnitExt*>(TechnoExt::Fetch(pThis->LinkedTo))->GetTypeExtData();
	auto slowdownCoefficient = pThis->movementspeed_50;
	const double mult = pTypeExt->CrushSlowdownMultiplier.Get(RulesExt::Global()->CrushSlowdownMultiplier);

	if (slowdownCoefficient > mult)
		slowdownCoefficient = mult;

	__asm { fld slowdownCoefficient };

	return SkipGameCode;
}

DEFINE_HOOK(0x6A108D, ShipLocomotionClass_WhileMoving_CrushTilt, 0xD)
{
	enum { SkipGameCode = 0x6A109A };

	GET(ShipLocomotionClass*, pThis, EBP);

	auto const pLinkedTo = pThis->LinkedTo;
	auto const pTypeExt = static_cast<UnitExt*>(TechnoExt::Fetch(pLinkedTo))->GetTypeExtData();
	pLinkedTo->RockingForwardsPerFrame = static_cast<float>(pTypeExt->CrushForwardTiltPerFrame.Get(-0.02));

	return SkipGameCode;
}

DEFINE_HOOK_AGAIN(0x6A0809, SomeLocomotionClass_WhileMoving_SkipCrushSlowDown, 0x6) // Ship
DEFINE_HOOK(0x4B1146, SomeLocomotionClass_WhileMoving_SkipCrushSlowDown, 0x6) // Drive
{
	GET(FootClass*, pLinkedTo, ECX);
	return static_cast<UnitExt*>(TechnoExt::Fetch(pLinkedTo))->GetTypeExtData()->SkipCrushSlowdown.Get(RulesExt::Global()->SkipCrushSlowdown) ? R->Origin() + 0x3C : 0;
}

/*
// Interceptamos el chequeo de aplastamiento para permitir que unidades con CrushAllInfantry 
// aplasten a cualquier infantería (salvo las inmunes/héroes), ignorando el flag Crushable=no, 
// pero protegiendo estrictamente a los tanques y vehículos.
DEFINE_HOOK(0x741890, UnitClass_CanCrush_CrushAllInfantry, 0x6) // Dirección estándar de validación de crush en UnitClass
{
	enum { DoNotCrush = 0, CanCrush = 1 };

	GET(UnitClass*, pThis, ESI);
	GET_STACK(AbstractClass*, pTarget, STACK_OFFSET(0x4, 0x0)); // El objeto que está siendo pisado

	if (!pThis || !pTarget)
		return 0; // Dejar que el juego decida normalmente

	// Verificamos si nuestra unidad tiene la extensión y la propiedad activada
	auto const pTypeExt = UnitTypeExt::Fetch(pThis->Type);
	if (pTypeExt && pTypeExt->CrushAllInfantry)
	{
		// Si el objetivo es infantería...
		if (pTarget->WhatAmI() == AbstractType::Infantry)
		{
			auto const pInf = static_cast<InfantryClass*>(pTarget);

			// Si la infantería es un héroe o tiene resistencia explícita al aplastamiento avanzado, respetamos su inmunidad
			// (p.ej. si usa OmniCrushResistant o es un héroe configurado para no ser aplastado)
			if (pInf->Type->Crushable) // O tu validación de héroes si usas una flag propia
			{
				// Dejamos que el motor aplaste a la infantería común
				return 0;
			}
			else
			{
				// CASO ESPECIAL: La infantería tiene Crushable=no (como la pesada), 
				// pero nuestro vehículo tiene CrushAllInfantry=yes. ¡Forzamos el aplastamiento!
				R->EAX(CanCrush);
				return 0x7418C5; // Salto directo a la validación de éxito de aplastamiento del motor
			}
		}
		else
		{
			// SI EL OBJETIVO ES UN VEHÍCULO/TANQUE: 
			// Forzamos a que NUNCA lo aplaste, sin importar si tenía OmniCrusher global activo.
			R->EAX(DoNotCrush);
			return 0x7418C5;
		}
	}

	return 0; // Comportamiento por default para el resto de unidades
}
*/
