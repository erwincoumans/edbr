#pragma once

#pragma once

#include <edbr/Math/Rect.h>
#include <edbr/UI/Element.h>
#include <edbr/UI/NineSliceElement.h>
#include <edbr/UI/TextLabel.h>

struct Font;

namespace ui
{

class TextButton : public Element {
public:
    struct Padding {
        float left{0.f};
        float right{0.f};
        float top{0.f};
        float bottom{0.f};
    };

public:
    TextButton(
        const ui::NineSliceStyle& nineSliceStyle,
        const Font& font,
        const std::string& label);
    TextButton(std::unique_ptr<NineSliceElement> nineSlice, std::unique_ptr<TextLabel> label);

    const NineSliceElement& getNineSlice() const;
    const TextLabel& getLabel() const;

    glm::vec2 getContentSize() const override;

private:
};

} // end of namespace ui
