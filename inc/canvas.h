#ifndef JOS_INC_CANVAS_H_
#define JOS_INC_CANVAS_H_

#include <inc/types.h>

typedef struct canvas
{
    uint16_t width;
    uint16_t height;
    uint8_t *data;
} canvas_t;

int canvas_init(uint16_t width, uint16_t height, canvas_t *canvas);
int canvas_draw_bg(uint8_t color, canvas_t *canvas);
int canvas_draw_ascii(uint16_t x, uint16_t y, char *str, uint8_t color, canvas_t *canvas);
int canvas_draw_cn(uint16_t x, uint16_t y, char *str, uint8_t color, canvas_t *canvas);
int canvas_draw_rect(uint16_t x, uint16_t y, uint16_t l, uint16_t w, uint8_t color, canvas_t *canvas);

#endif /* JOS_INC_CANVAS_H_ */