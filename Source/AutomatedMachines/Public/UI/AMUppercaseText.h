// A text field that writes in capitals while typing (like CSS text-transform: uppercase).
#pragma once

#include "CoreMinimal.h"
#include "Components/EditableText.h"
#include "AMUppercaseText.generated.h"

UCLASS()
class AUTOMATEDMACHINES_API UAMUppercaseText : public UEditableText
{
	GENERATED_BODY()

public:
	/**
	 * If the text has small letters, replaces it with the capitalised text and puts the cursor at the
	 * end (where the player was typing). Returns true if it changed the text. Call from OnTextChanged.
	 */
	bool MakeUppercase();
};
