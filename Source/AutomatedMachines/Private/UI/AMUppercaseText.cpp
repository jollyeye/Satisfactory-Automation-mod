#include "UI/AMUppercaseText.h"
#include "Widgets/Input/SEditableText.h"

bool UAMUppercaseText::MakeUppercase()
{
	if (!MyEditableText.IsValid())
	{
		return false;
	}
	const FString Current = MyEditableText->GetText().ToString();
	const FString Upper = Current.ToUpper();
	if (Current.Equals(Upper, ESearchCase::CaseSensitive))
	{
		return false;
	}
	// Straight on the Slate widget: it updates what is shown at once, even while typing.
	MyEditableText->SetText(FText::FromString(Upper));
	MyEditableText->GoTo(ETextLocation::EndOfDocument);
	return true;
}
