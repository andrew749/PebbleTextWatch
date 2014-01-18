#include "pebble.h"
#include "num2words-en.h"

#define DEBUG 1
#define BUFFER_SIZE 44

static Window *window;
//change to 2 subjayers one for date and one for time
typedef struct {
	TextLayer *currentLayer;
	TextLayer *nextLayer;
	//add date layer
	PropertyAnimation *currentAnimation;
	PropertyAnimation *nextAnimation;
} Line;

static Line line1;
static Line line2;
static Line line3;

static struct tm *t;
static GFont lightFont;
static GFont boldFont;

static char line1Str[2][BUFFER_SIZE];
static char line2Str[2][BUFFER_SIZE];
static char line3Str[2][BUFFER_SIZE];
static char date_text[] = "Xxxxxxxxx 00000";
 //holds the format of the date to be displayed
static char date_format[]="%B %e %G";
//added to include a new layer to the watchface
static TextLayer *text_date_layer;
Layer *line_layer;

// Animation handler
static void animationStoppedHandler(struct Animation *animation, bool finished,
		void *context) {
	Layer *textLayer = text_layer_get_layer((TextLayer *) context);
	GRect rect = layer_get_frame(textLayer);
	rect.origin.x = 144;
	layer_set_frame(textLayer, rect);
}

// Animate line
static void makeAnimationsForLayers(Line *line, TextLayer *current,
		TextLayer *next) {
	GRect fromRect = layer_get_frame(text_layer_get_layer(next));
	GRect toRect = fromRect;
	toRect.origin.x -= 144;

	line->nextAnimation = property_animation_create_layer_frame(
			text_layer_get_layer(next), &fromRect, &toRect);
	animation_set_duration((Animation *) line->nextAnimation, 400);
	animation_set_curve((Animation *) line->nextAnimation,
			AnimationCurveEaseOut);
	animation_schedule((Animation *) line->nextAnimation);

	GRect fromRect2 = layer_get_frame(text_layer_get_layer(current));
	GRect toRect2 = fromRect2;
	toRect2.origin.x -= 144;

	line->currentAnimation = property_animation_create_layer_frame(
			text_layer_get_layer(current), &fromRect2, &toRect2);
	animation_set_duration((Animation *) line->currentAnimation, 400);
	animation_set_curve((Animation *) line->currentAnimation,
			AnimationCurveEaseOut);

	animation_set_handlers((Animation *) line->currentAnimation,
			(AnimationHandlers ) { .stopped =
							(AnimationStoppedHandler) animationStoppedHandler },
			current);

	animation_schedule((Animation *) line->currentAnimation);
}

// Update line
static void updateLineTo(Line *line, char lineStr[2][BUFFER_SIZE], char *value) {
	TextLayer *next, *current;

	GRect rect = layer_get_frame(text_layer_get_layer(line->currentLayer));
	current = (rect.origin.x == 0) ? line->currentLayer : line->nextLayer;
	next = (current == line->currentLayer) ?
			line->nextLayer : line->currentLayer;

	// Update correct text only
	if (current == line->currentLayer) {
		memset(lineStr[1], 0, BUFFER_SIZE);
		memcpy(lineStr[1], value, strlen(value));
		text_layer_set_text(next, lineStr[1]);
	} else {
		memset(lineStr[0], 0, BUFFER_SIZE);
		memcpy(lineStr[0], value, strlen(value));
		text_layer_set_text(next, lineStr[0]);
	}

	makeAnimationsForLayers(line, current, next);
}

// Check to see if the current line needs to be updated
static bool needToUpdateLine(Line *line, char lineStr[2][BUFFER_SIZE],
		char *nextValue) {
	char *currentStr;
	GRect rect = layer_get_frame(text_layer_get_layer(line->currentLayer));
	currentStr = (rect.origin.x == 0) ? lineStr[0] : lineStr[1];

	if (memcmp(currentStr, nextValue, strlen(nextValue)) != 0
			|| (strlen(nextValue) == 0 && strlen(currentStr) != 0)) {
		return true;
	}
	return false;
}

// Update screen based on new time
static void display_time(struct tm *t) {
	// The current time text will be stored in the following 3 strings
	char textLine1[BUFFER_SIZE];
	char textLine2[BUFFER_SIZE];
	char textLine3[BUFFER_SIZE];
	time_to_3words(t->tm_hour, t->tm_min, textLine1, textLine2, textLine3,
	BUFFER_SIZE);

	if (needToUpdateLine(&line1, line1Str, textLine1)) {
		updateLineTo(&line1, line1Str, textLine1);
	}
	if (needToUpdateLine(&line2, line2Str, textLine2)) {
		updateLineTo(&line2, line2Str, textLine2);
	}
	if (needToUpdateLine(&line3, line3Str, textLine3)) {
		updateLineTo(&line3, line3Str, textLine3);
	}
	strftime(date_text, sizeof(date_text), date_format, t);
	text_layer_set_text(text_date_layer, date_text);

}

// Update screen without animation first time we start the watchface
static void display_initial_time(struct tm *t) {
	time_to_3words(t->tm_hour, t->tm_min, line1Str[0], line2Str[0], line3Str[0],
	BUFFER_SIZE);
	strftime(date_text, sizeof(date_text), date_format, t);

	text_layer_set_text(line1.currentLayer, line1Str[0]);
	text_layer_set_text(line2.currentLayer, line2Str[0]);
	text_layer_set_text(line3.currentLayer, line3Str[0]);

	text_layer_set_text(text_date_layer, date_text);
}

// Debug methods. For quickly debugging enable debug macro on top to transform the watchface into
// a standard app and you will be able to change the time with the up and down buttons
#if DEBUG

static void up_single_click_handler(ClickRecognizerRef recognizer,
		void *context) {
	t->tm_min += 1;
	if (t->tm_min >= 60) {
		t->tm_min = 0;
		t->tm_hour += 1;

		if (t->tm_hour >= 24) {
			t->tm_hour = 0;
		}
	}
	display_time(t);
}

static void down_single_click_handler(ClickRecognizerRef recognizer,
		void *context) {
	t->tm_min -= 1;
	if (t->tm_min < 0) {
		t->tm_min = 59;
		t->tm_hour -= 1;
	}
	display_time(t);
}

static void click_config_provider(ClickRecognizerRef recognizer, void *context) {
	window_single_click_subscribe(BUTTON_ID_UP,
			(ClickHandler) up_single_click_handler);
	window_single_click_subscribe(BUTTON_ID_DOWN,
			(ClickHandler) down_single_click_handler);
}

#endif

// Configure the first line of text
static void configureBoldLayer(TextLayer *textlayer) {
	text_layer_set_font(textlayer, boldFont);
	text_layer_set_text_color(textlayer, GColorWhite);
	text_layer_set_background_color(textlayer, GColorClear);
	text_layer_set_text_alignment(textlayer, GTextAlignmentLeft);
}

// Configure for the 2nd and 3rd lines
static void configureLightLayer(TextLayer *textlayer) {
	text_layer_set_font(textlayer, lightFont);
	text_layer_set_text_color(textlayer, GColorWhite);
	text_layer_set_background_color(textlayer, GColorClear);
	text_layer_set_text_alignment(textlayer, GTextAlignmentLeft);
}

// Time handler called every minute by the system
static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
	t = tick_time;
	display_time(tick_time);
}
void line_layer_update_callback(Layer *layer, GContext* ctx) {
	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}
static void init() {
	window = window_create();
	window_stack_push(window, true);
	window_set_background_color(window, GColorBlack);

	// Load layers
	Layer *window_layer = window_get_root_layer(window);
	Layer *text_layer = layer_create(GRect(0, 0, 144, 168 - 48));

	// Custom fonts
	lightFont = fonts_load_custom_font(
			resource_get_handle(RESOURCE_ID_FONT_GOTHAM_LIGHT_31));
	boldFont = fonts_load_custom_font(
			resource_get_handle(RESOURCE_ID_FONT_GOTHAM_BOLD_36));

	// 1st line layers
	line1.currentLayer = text_layer_create(GRect(0, 0, 144, 40));
	line1.nextLayer = text_layer_create(GRect(144, 0, 144, 40));
	configureBoldLayer(line1.currentLayer);
	configureBoldLayer(line1.nextLayer);

	// 2nd layers
	line2.currentLayer = text_layer_create(GRect(0, 40, 144, 40));
	line2.nextLayer = text_layer_create(GRect(144, 40, 144, 40));
	configureLightLayer(line2.currentLayer);
	configureLightLayer(line2.nextLayer);

	// 3rd layers
	line3.currentLayer = text_layer_create(GRect(0, 80, 144, 50));
	line3.nextLayer = text_layer_create(GRect(144, 80, 144, 50));
	configureLightLayer(line3.currentLayer);
	configureLightLayer(line3.nextLayer);

	//date area
	text_date_layer = text_layer_create(GRect(8, 130, 144 - 8, 168 - 130));
	configureLightLayer(text_date_layer);
	text_layer_set_font(text_date_layer,
			fonts_load_custom_font(
					resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_19)));

	//separating line
	GRect line_frame = GRect(8, 130, 139, 2);
	line_layer = layer_create(line_frame);
	layer_set_update_proc(line_layer, line_layer_update_callback);
	layer_add_child(window_layer, line_layer);

	// Configure time on init
	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	display_initial_time(t);

	//break up the layers into date layer and actual time layer
	layer_add_child(window_layer, text_layer);

	//add the 3 word layers to the text layer
	layer_add_child(text_layer, text_layer_get_layer(line1.currentLayer));
	layer_add_child(text_layer, text_layer_get_layer(line1.nextLayer));
	layer_add_child(text_layer, text_layer_get_layer(line2.currentLayer));
	layer_add_child(text_layer, text_layer_get_layer(line2.nextLayer));
	layer_add_child(text_layer, text_layer_get_layer(line3.currentLayer));
	layer_add_child(text_layer, text_layer_get_layer(line3.nextLayer));

	//add the date layer to the watchface
	layer_add_child(window_layer, text_layer_get_layer(text_date_layer));

#if DEBUG
	// Button functionality
	window_set_click_config_provider(window,
			(ClickConfigProvider) click_config_provider);
#endif

	tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
}

static void deinit() {
	tick_timer_service_unsubscribe();
	window_destroy(window);
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}
