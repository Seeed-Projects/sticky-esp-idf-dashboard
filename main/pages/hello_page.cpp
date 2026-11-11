#include "hello_page.h"

#include "app_state.h"
#include "canvas.h"
#include "page.h"

void hello_page_render(Canvas &canvas, const AppState &state)
{
    page_draw_layout(canvas, state, "Hello Sticky", "My First Page");
    canvas.draw_text(76, 250, "ESP-IDF Ready", 3, GrayLevel::Black);
    canvas.draw_line(76, 310, 718, 310, GrayLevel::Black);
}
