#include "Body.h"

#include <Ext/InfantryType/Body.h>
extern std::unordered_map<InfantryClass*, int> RocketeersEnCaida;

InfantryExt::ExtContainer InfantryExt::ExtMap;

// Returns hardcoded prone/deployed FLH overrides for infantry, if set.
CoordStruct InfantryExt::GetSimpleFLH(InfantryClass* pThis, int weaponIndex, bool& FLHFound)
{
	FLHFound = false;
	CoordStruct FLH = CoordStruct::Empty;

	auto const pTypeExt = InfantryTypeExt::Fetch(pThis->Type);
	Nullable<CoordStruct> pickedFLH;

	if (pThis->IsDeployed())
	{
		if (weaponIndex == 0)
			pickedFLH = pTypeExt->DeployedPrimaryFireFLH;
		else if (weaponIndex == 1)
			pickedFLH = pTypeExt->DeployedSecondaryFireFLH;
	}
	else
	{
		if (pThis->Crawling)
		{
			if (weaponIndex == 0)
				pickedFLH = pTypeExt->PronePrimaryFireFLH;
			else if (weaponIndex == 1)
				pickedFLH = pTypeExt->ProneSecondaryFireFLH;
		}
	}

	if (pickedFLH.isset())
	{
		FLH = pickedFLH.Get();
		FLHFound = true;
	}

	return FLH;
}

// =============================
// load / save

template <typename T>
void InfantryExt::Serialize(T& Stm)
{
	Stm
		.Process(this->SkipTargetChangeResetSequence)
		.Process(this->HasDeployConverted)
		.Process(this->HasUndeployConverted)
		;
}

void InfantryExt::LoadFromStream(PhobosStreamReader& Stm)
{
	FootExt::LoadFromStream(Stm);
	this->Serialize(Stm);
}

void InfantryExt::SaveToStream(PhobosStreamWriter& Stm)
{
	FootExt::SaveToStream(Stm);
	this->Serialize(Stm);
}

// =============================
// container

InfantryExt::ExtContainer::ExtContainer() : Container("InfantryClass") { }
InfantryExt::ExtContainer::~ExtContainer() = default;

// =============================
// container hooks

DEFINE_HOOK(0x517A60, InfantryClass_CTOR, 0xE)
{
	GET(InfantryClass*, pItem, ESI);

	InfantryExt::ExtMap.Allocate(pItem);

	return 0;
}

// Late in every destructor body of the class, right before it chains into the
// base destructor: the last point where the extension is no longer used.
/*DEFINE_HOOK(0x517F81, InfantryClass_DTOR, 0x8)
{
	GET(InfantryClass*, pItem, ESI);



	// Tu código original de Phobos
	InfantryExt::ExtMap.Remove(pItem);

	return 0;
}*/

// ========================================================================
// LA RESURRECCIÓN: CUANDO TOCA EL PISO Y SE DESTRUYE
// ========================================================================
DEFINE_HOOK(0x517F81, InfantryClass_DTOR, 0x8)
{
	GET(InfantryClass*, pItem, ESI);

	// Buscamos si el soldado que acaba de estrellarse era uno de los nuestros
	auto it = RocketeersEnCaida.find(pItem);
	if (it != RocketeersEnCaida.end())
	{
		// Rescatamos su vida original
		int saludOriginal = it->second;

		// Lo sacamos de la sala de espera
		RocketeersEnCaida.erase(it);

		// Instanciamos a su clon terrestre
		InfantryTypeClass* pGroundType = InfantryTypeClass::Find("JUMPJET_G");
		if (pGroundType)
		{
			InfantryClass* pClone = reinterpret_cast<InfantryClass*>(pGroundType->CreateObject(pItem->Owner));
			if (pClone)
			{
				pClone->Health = saludOriginal;
				pClone->Veterancy = pItem->Veterancy; // Todavía podemos leer su rango antes de que se borre

				// Clavamos Z a 0 por seguridad y lo spawneamos en el lugar del impacto
				CoordStruct myCoords = pItem->GetCoords();
				myCoords.Z = 0;
				pClone->ForceCreate(myCoords, 0);
			}
		}
	}

	// Código original de Phobos para limpiar las extensiones
	InfantryExt::ExtMap.Remove(pItem);

	return 0;
}
