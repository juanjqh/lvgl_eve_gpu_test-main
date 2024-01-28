#if ARUDINO
#include <Arduino.h>
#include "SPI.h"
#endif
#include "src/draw/eve/eve_ram_g.h"
#include "src/draw/eve/lv_eve.h"
#include "EVE.h"
#include "tft_eve_init.h"
#include "lvgl.h"
#include "src/draw/eve/lv_draw_eve.h"
#include "../lvgl/examples/lv_examples.h"
#include "../lvgl/demos/lv_demos.h"

uint16_t X;
uint16_t Y;
uint16_t Width;
uint16_t Height;

uint16_t list_size = 0;
uint16_t cmd_size = 0;

void eve_display_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p);
static void eve_touch_read(lv_indev_t *drv, lv_indev_data_t *data);

/* Serial debugging */
void my_print(lv_log_level_t level, const char *buf)
{
    LV_UNUSED(level);
#if ARUDINO
    Serial.println(buf);
    Serial.flush();
#endif
}

void setup()
{
#if ARUDINO /*SPI init*/
    pinMode(EVE_CS, OUTPUT);
    digitalWrite(EVE_CS, HIGH);
    pinMode(EVE_PDN, OUTPUT);
    digitalWrite(EVE_PDN, LOW);
    Serial.begin(115200); /* prepare for possible serial debug */
    SPI.begin();          /* sets up the SPI to run in Mode 0 and 1 MHz */
    SPI.beginTransaction(SPISettings(24000000, MSBFIRST, SPI_MODE0));
#endif

    lv_init(); /* LVGL Init */
#if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif
    lv_tick_set_cb((lv_tick_get_cb_t)millis);
 
    TFT_init(); /*Init EVE display*/

    EVE_start_cmd_burst();
    EVE_cmd_dl_burst(CMD_DLSTART); /* start the display list */
    EVE_cmd_dl_burst(DL_CLEAR_COLOR_RGB | 0x000000);
    EVE_cmd_dl_burst(DL_CLEAR | CLR_COL | CLR_STN | CLR_TAG);
    EVE_cmd_dl_burst(VERTEX_FORMAT(0));
    // EVE_cmd_dl_burst(VERTEX_TRANSLATE_X(8));
    // EVE_cmd_dl_burst(VERTEX_TRANSLATE_Y(8));

#if (!LV_USE_DRAW_EVE)

    EVE_cmd_setbitmap_burst(0, EVE_RGB565, EVE_HSIZE, EVE_VSIZE);
    EVE_cmd_dl_burst(DL_BEGIN | EVE_BITMAPS);
    EVE_cmd_dl_burst(VERTEX2F(0, 0));
    EVE_cmd_dl(DL_END);
    EVE_cmd_dl_burst(DL_DISPLAY); /* instruct the co-processor to show the list */
    EVE_cmd_dl_burst(CMD_SWAP);   /* make this list active */

#endif
    static uint8_t buf1[EVE_HSIZE * 40];
    lv_display_t *disp_;
    disp_ = lv_display_create(EVE_HSIZE, EVE_VSIZE);
    lv_display_set_flush_cb(disp_, eve_display_flush);
#if (!LV_USE_DRAW_EVE)
    lv_display_set_buffers(disp_, buf1, NULL, EVE_HSIZE * 40, LV_DISP_RENDER_MODE_PARTIAL);
#else
    lv_display_set_buffers(disp_, buf1, NULL, EVE_HSIZE * 40, LV_DISP_RENDER_MODE_PARTIAL);
    lv_display_set_render_mode(disp_, LV_DISPLAY_RENDER_MODE_FULL); /*Force to use LV_DISPLAY_RENDER_MODE_FULL*/
#endif

    /*Register a touchpad input device*/
    lv_indev_t *indev_touchpad;
    indev_touchpad = lv_indev_create();
    lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev_touchpad, eve_touch_read);

    /*********************************************************************************************************************/
    // lv_example_scroll_1();
    // lv_example_scroll_2();
    // lv_example_scroll_3();
    // lv_example_scroll_4();
    // lv_example_scroll_5();
    // lv_example_scroll_6();
    // lv_example_style_3();
    // lv_example_style_6();
    // lv_example_textarea_2();
    // lv_example_label_3();
    // lv_animimag();
    // lv_example_style_8();
    // lv_example_scroll_6();
    lv_example_menu_5(); /***/
                         // lv_example_anim_timeline_1();
                         // lv_example_label_3();
                         // lv_example_spinner_1();
                         // lv_example_canvas_3();
                         // lv_example_get_started_4();
                         // lv_demo_widgets();
                         // lv_demo_benchmark();
                         // lv_demo_music();
    while (1)
    {
        lv_timer_handler();
    }
}

void loop()
{
    // lv_timer_handler();
}

/* Display flushing */
void eve_display_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p)
{

#if !LV_USE_DRAW_EVE

    X = area->x1;
    Y = area->y1;
    Width = lv_area_get_width(area);
    Height = lv_area_get_height(area);

#define BYTES_PER_PIXEL (16 / 8) // bytes per pixel for (16 for RGB565)
#define BYTES_PER_LINE (EVE_HSIZE * BYTES_PER_PIXEL)
#define SCREEN_BUFFER_SIZE (EVE_HSIZE * EVE_VSIZE * BYTES_PER_PIXEL)

    uint32_t addr = 0 + (Y * BYTES_PER_LINE) + (X * BYTES_PER_PIXEL);

    for (uint16_t vert = 0; vert < Height; vert++)
    {

        EVE_memWrite_sram_buffer(addr, (uint8_t *)color_p + (Width * BYTES_PER_PIXEL * vert), Width * BYTES_PER_PIXEL);

        addr += BYTES_PER_LINE;
    }

#else

    EVE_end_cmd_burst(); /* stop writing to the cmd-fifo, the cmd-FIFO will be executed automatically after this or when DMA is done */

    list_size = EVE_memRead16(REG_CMD_DL); /* debug-information, get the size of the last generated display-list */
    EVE_start_cmd_burst();

    EVE_cmd_dl_burst(COLOR_RGB(0, 0, 0));
    eve_scissor(0, 0, EVE_HSIZE, EVE_VSIZE);
    EVE_cmd_text_burst(760, 17, 26, EVE_OPT_RIGHTX, "DL:");
    EVE_cmd_number_burst(795, 17, 26, EVE_OPT_RIGHTX, list_size);

    EVE_cmd_dl_burst(DL_DISPLAY); /* instruct the co-processor to show the list */
    EVE_cmd_dl_burst(CMD_SWAP);   /* make this list active */
    EVE_end_cmd_burst();
    EVE_execute_cmd();

    EVE_start_cmd_burst();
    EVE_cmd_dl_burst(CMD_DLSTART);
    EVE_cmd_dl_burst(DL_CLEAR | CLR_COL | CLR_STN | CLR_TAG);
    EVE_cmd_dl_burst(VERTEX_FORMAT(0));
    /* use vertex translate 8 ? */
    // EVE_cmd_dl_burst(VERTEX_TRANSLATE_X(8));
    // EVE_cmd_dl_burst(VERTEX_TRANSLATE_Y(8));

#endif

    lv_display_flush_ready(disp);
}

static void eve_touch_read(lv_indev_t *drv, lv_indev_data_t *data)
{
    static int16_t last_x = 0;
    static int16_t last_y = 0;
    bool touched = true;
    EVE_end_cmd_burst();
    uint32_t XY = EVE_memRead32(REG_TOUCH_SCREEN_XY);

    uint16_t Y = XY & 0xffff;
    uint16_t X = XY >> 16;

    // is it not touched (or invalid because of calibration range)
    if (X == 0x8000 || Y == 0x8000 || X > 800 || Y > 480)
    {
        touched = false;
        X = last_x;
        Y = last_y;
    }
    else
    {
        last_x = X;
        last_y = Y;
    }

    data->point.x = X;
    data->point.y = Y;
    data->state = (touched == false ? LV_INDEV_STATE_REL : LV_INDEV_STATE_PR);
    EVE_start_cmd_burst();
}
