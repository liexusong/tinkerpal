/* Copyright (c) 2013, Eyal Birger
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The name of the author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "util/debug.h"
#include "platform/platform_consts.h"
#include "boards/board.h"
#include "apps/cli.h"
#include "graphics/graphics.h"
#include "graphics/colors.h"

static canvas_t *canvas;

static void graphics_test_process_line(tstr_t *line)
{
    if (!tstr_cmp(line, &S("fill")))
    {
	int i, j;

	for (i = 0; i < canvas->width; i++)
	{
	    for (j = 0; j < canvas->height; j++)
		canvas_pixel_set(canvas, i, j, (i * j) & 0xffff);
	}
    }
    if (!tstr_cmp(line, &S("clear")))
    {
	/* XXX: When rect API is available, use it */
	int i, j;

	for (i = 0; i < canvas->width; i++)
	{
	    for (j = 0; j < canvas->height; j++)
		canvas_pixel_set(canvas, i, j, 0);
	}
    }
    if (!tstr_cmp(line, &S("circle")))
    {
	circle_draw(canvas, canvas->width / 2, canvas->height / 2,
	    canvas->width / 4, 0xffff);
    }
    if (!tstr_cmp(line, &S("text")))
	string_draw(canvas, 10, 10, &S("Hello TinkerPal"), 0xffff);

    console_printf("Ok\n");
}

static cli_client_t graphics_test_cli_client = {
    .process_line = graphics_test_process_line,
};

#ifdef CONFIG_ILI93XX
static void lcd_init(void)
{
    canvas = ili93xx_new(&board.ili93xx_params);
}
#else
#error No LCD hookup information available
#endif

void app_start(int argc, char *argv[])
{
    tp_out(("TinkerPal Application - Graphics Test\n"));

    lcd_init();

    cli_start(&graphics_test_cli_client);
}