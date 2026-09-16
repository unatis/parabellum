// Редакторские команды (только WITH_EDITOR). Вызываются из Python-коммандлетов через execute_console_command,
// когда нужного API нет в Python-рефлексии.
#if WITH_EDITOR

#include "Animation/Skeleton.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

/**
 * pbl.Anim.FixRetarget <skeleton asset path> [pelvisBone=Hips]
 * Анимации Mixamo несут длины костей своего рига - шея/конечности растягиваются. Ставим ретаргет трансляций:
 * корень - Animation, таз - AnimationScaled, остальные - Skeleton (длины костей из скелета меша). Сохраняет ассет.
 */
static FAutoConsoleCommand CmdFixRetarget(
	TEXT("pbl.Anim.FixRetarget"),
	TEXT("Set bone translation retargeting (root=Animation, pelvis=AnimationScaled, others=Skeleton) and save: pbl.Anim.FixRetarget <skeleton path> [pelvis bone]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 1) { UE_LOG(LogTemp, Warning, TEXT("usage: pbl.Anim.FixRetarget <skeleton path> [pelvis]")); return; }
		USkeleton* Skel = LoadObject<USkeleton>(nullptr, *Args[0]);
		if (!Skel) { UE_LOG(LogTemp, Error, TEXT("FixRetarget: skeleton '%s' not found"), *Args[0]); return; }
		const FName Pelvis = Args.Num() > 1 ? FName(*Args[1]) : FName(TEXT("Hips"));
		const FReferenceSkeleton& Ref = Skel->GetReferenceSkeleton();
		int32 NumSkel = 0, NumScaled = 0;
		for (int32 i = 0; i < Ref.GetNum(); ++i)
		{
			EBoneTranslationRetargetingMode::Type Mode = EBoneTranslationRetargetingMode::Skeleton;
			if (i == 0) { Mode = EBoneTranslationRetargetingMode::Animation; }
			else if (Ref.GetBoneName(i) == Pelvis) { Mode = EBoneTranslationRetargetingMode::AnimationScaled; ++NumScaled; }
			else { ++NumSkel; }
			Skel->SetBoneTranslationRetargetingMode(i, Mode, false);
		}
		Skel->MarkPackageDirty();
		UPackage* Pkg = Skel->GetOutermost();
		const FString FileName = FPackageName::LongPackageNameToFilename(Pkg->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		const bool bSaved = UPackage::SavePackage(Pkg, Skel, *FileName, SaveArgs);
		UE_LOG(LogTemp, Display, TEXT("FixRetarget: %s - %d bones Skeleton, %d AnimationScaled (%s), root Animation; saved=%d -> %s"),
			*Skel->GetName(), NumSkel, NumScaled, *Pelvis.ToString(), bSaved ? 1 : 0, *FileName);
	}));

#endif
