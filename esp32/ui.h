#ifndef UI_H
#define UI_H

#define LV_LVGL_H_INCLUDE_SIMPLE
#include "config.h"
#include "lcd.h"
#include "local_fonts.h"
#include "theme.h"

#include "FS.h"
#include "SPIFFS.h"
#include "ble.h"
#include <lvgl.h>

#define FS                      SPIFFS
#define FORMAT_SPIFFS_IF_FAILED true

#define ICON_HEIGHT             62
#define ICON_WIDTH              64
#define ICON_BITMAP_BUFFER_SIZE (ICON_HEIGHT * ICON_WIDTH / 8)
#define ICON_RENDER_BUFFER_SIZE (ICON_BITMAP_BUFFER_SIZE * LV_COLOR_DEPTH)

#ifdef HORIZONTAL
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 172
#else
#define SCREEN_WIDTH  172
#define SCREEN_HEIGHT 320
#endif

#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT)
uint16_t draw_buf_0[DRAW_BUF_SIZE + 10 /* padding, just in case */];


SimpleSt7789 lcd(&SPI,
                 SPISettings(80000000, MSBFIRST, SPI_MODE0),
                 SCREEN_HEIGHT,
                 SCREEN_HEIGHT,
                 PIN_LCD_CS,
                 PIN_LCD_DC,
                 PIN_LCD_RST,
                 PIN_BACKLIGHT,
#ifdef HORIZONTAL
                 SimpleSt7789::ROTATION_270
#else
                 SimpleSt7789::ROTATION_180
#endif
);


#if LV_USE_LOG != 0
void my_print(lv_log_level_t level, const char* buf) {
	LV_UNUSED(level);
	Serial.println(buf);
	Serial.flush();
}
#endif

void my_disp_flush(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
	lcd.flushWindow(area->x1, area->y1, area->x2, area->y2, (uint16_t*)px_map);
	lv_display_flush_ready(disp);
}

static uint32_t my_tick(void) {
	return millis();
}

namespace Data {
	namespace details {
		int speed                 = -1;
		String nextRoad           = String();
		String nextRoadDesc       = String();
		String eta                = String();
		String ete                = String();
		String distanceToNextTurn = String();
		String totalDistance      = String();
		String displayIconHash    = String(); // empty if no icon to display
		String receivedIconHash   = String(); // empty if no icon received
		bool iconDirty            = false;    // true if icon needs to be rendered
		std::vector<String> availableIcons{};
		uint8_t receivedIconBitmapBuffer[ICON_BITMAP_BUFFER_SIZE]; // for receiving from BLE
		uint8_t iconBitmapBuffer[ICON_BITMAP_BUFFER_SIZE];         // for loading from FS
		uint8_t iconRenderBuffer[ICON_RENDER_BUFFER_SIZE];         // for rendering
	} // namespace details
} // namespace Data

namespace UI {
	namespace details {
		lv_obj_t* lblSpeed;
		lv_obj_t* lblSpeedUnit;
		lv_obj_t* lblEta;
		lv_obj_t* lblNextRoad;
		lv_obj_t* lblNextRoadDesc;
		lv_obj_t* lblDistanceToNextRoad;
		lv_obj_t* imgTbtIcon;

		uint32_t lastUpdate = 0;
	} // namespace details

	void init() {
		using namespace details;

		SPI.begin(PIN_SCLK, PIN_MISO, PIN_MOSI);
#ifdef HORIZONTAL
		lcd.setOffset(0, 34);
#else
		lcd.setOffset(34, 0);
#endif

		lcd.init();

		lv_init();
		lv_tick_set_cb(my_tick);

		delay(100);
		memset(draw_buf_0, 0xAA, sizeof(draw_buf_0));
		lcd.flushWindow(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, draw_buf_0);
		delay(200);

#if LV_USE_LOG != 0
		lv_log_register_print_cb(my_print);
#endif

		lv_display_t* disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
		lv_display_set_flush_cb(disp, my_disp_flush);
		lv_display_set_buffers(disp, draw_buf_0, nullptr, sizeof(draw_buf_0), LV_DISPLAY_RENDER_MODE_FULL);

		lv_obj_set_style_bg_color(lv_scr_act(), lv_color_make(0xFF, 0xFF, 0xFF), LV_PART_MAIN);

		imgTbtIcon = lv_img_create(lv_scr_act());
		lv_obj_set_style_bg_color(imgTbtIcon, lv_color_make(0xFF, 0xFF, 0xFF), LV_PART_MAIN);

		lblSpeed = lv_label_create(lv_scr_act());
		lv_label_set_text(lblSpeed, "0");
		lv_obj_set_style_text_color(lblSpeed, lv_color_make(0xFF, 0x00, 0x00), LV_PART_MAIN);

		lblSpeedUnit = lv_label_create(lv_scr_act());
		lv_label_set_text(lblSpeedUnit, "km/h");

		lblDistanceToNextRoad = lv_label_create(lv_scr_act());
		lv_label_set_text(lblDistanceToNextRoad, "CatDrive");
		lv_obj_set_style_text_color(lblDistanceToNextRoad, lv_color_make(0x00, 0x00, 0xff), LV_PART_MAIN);

		lblNextRoad = lv_label_create(lv_scr_act());
		lv_label_set_text(lblNextRoad, "Welcome!");

		lblNextRoadDesc = lv_label_create(lv_scr_act());
		lv_label_set_text(lblNextRoadDesc, "");
		lv_obj_set_style_text_color(lblNextRoadDesc, lv_color_make(0x55, 0x55, 0x55), LV_PART_MAIN);

		lblEta = lv_label_create(lv_scr_act());
		lv_label_set_text(lblEta, "");
		lv_obj_set_style_text_color(lblEta, lv_color_make(0x55, 0x55, 0x55), LV_PART_MAIN);

#ifdef HORIZONTAL
#define LEFT_PART_WIDTH  (SCREEN_HEIGHT / 2 - 12)
#define RIGHT_PART_WIDTH (SCREEN_WIDTH - LEFT_PART_WIDTH - 10)

		// Image top left
		lv_obj_set_style_width(imgTbtIcon, ICON_WIDTH, LV_PART_MAIN);
		lv_obj_set_style_height(imgTbtIcon, ICON_HEIGHT, LV_PART_MAIN);
		lv_obj_align(imgTbtIcon, LV_ALIGN_TOP_LEFT, 10, 10);

		lv_label_set_long_mode(lblSpeed, LV_LABEL_LONG_SCROLL_CIRCULAR);
		lv_obj_set_style_width(lblSpeed, LEFT_PART_WIDTH, LV_PART_MAIN);
		lv_obj_set_style_text_font(lblSpeed, get_roboto_number_bold_48(), LV_STATE_DEFAULT);
		lv_obj_set_style_text_align(lblSpeed, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
		lv_obj_align(lblSpeed, LV_ALIGN_BOTTOM_LEFT, 12, -10);

		lv_obj_set_style_width(lblSpeedUnit, LEFT_PART_WIDTH, LV_PART_MAIN);
		lv_obj_set_style_text_font(lblSpeedUnit, get_roboto_24(), LV_STATE_DEFAULT);
		lv_obj_set_style_text_align(lblSpeedUnit, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
		lv_obj_align_to(lblSpeedUnit, lblSpeed, LV_ALIGN_TOP_LEFT, 0, -28);

		lv_label_set_long_mode(lblEta, LV_LABEL_LONG_SCROLL_CIRCULAR);
		lv_obj_set_style_width(lblEta, RIGHT_PART_WIDTH, LV_PART_MAIN);
		lv_obj_set_style_text_font(lblEta, get_roboto_24(), LV_STATE_DEFAULT);
		lv_obj_set_style_text_align(lblEta, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
		lv_obj_align(lblEta, LV_ALIGN_TOP_RIGHT, 0, 10);

		lv_label_set_long_mode(lblDistanceToNextRoad, LV_LABEL_LONG_SCROLL_CIRCULAR);
		lv_obj_set_style_width(lblDistanceToNextRoad, RIGHT_PART_WIDTH, LV_PART_MAIN);
		lv_obj_set_style_text_font(lblDistanceToNextRoad, get_roboto_bold_32(), LV_STATE_DEFAULT);
		lv_obj_set_style_text_align(lblDistanceToNextRoad, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
		lv_obj_align_to(lblDistanceToNextRoad, lblEta, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

		lv_label_set_long_mode(lblNextRoadDesc, LV_LABEL_LONG_SCROLL_CIRCULAR);
		lv_obj_set_style_width(lblNextRoadDesc, RIGHT_PART_WIDTH, LV_PART_MAIN);
		lv_obj_set_style_text_font(lblNextRoadDesc, get_roboto_semibold_24(), LV_STATE_DEFAULT);
		lv_obj_set_style_text_align(lblNextRoadDesc, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
		lv_obj_align(lblNextRoadDesc, LV_ALIGN_BOTTOM_RIGHT, 0, -10);

		lv_label_set_long_mode(lblNextRoad, LV_LABEL_LONG_SCROLL_CIRCULAR);
		lv_obj_set_style_width(lblNextRoad, RIGHT_PART_WIDTH, LV_PART_MAIN);
		lv_obj_set_style_text_font(lblNextRoad, get_roboto_semibold_28(), LV_STATE_DEFAULT);
		lv_obj_set_style_text_align(lblNextRoad, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
		lv_obj_align_to(lblNextRoad, lblNextRoadDesc, LV_ALIGN_TOP_LEFT, 0, -40);

#else
		lv_obj_set_style_width(imgTbtIcon, ICON_WIDTH, LV_PART_MAIN);
		lv_obj_set_style_height(imgTbtIcon, ICON_HEIGHT, LV_PART_MAIN);
		lv_obj_align(imgTbtIcon, LV_ALIGN_TOP_LEFT, 10, 10);

		lv_label_set_long_mode(lblSpeed, LV_LABEL_LONG_SCROLL_CIRCULAR);
		lv_obj_set_style_width(lblSpeed, SCREEN_WIDTH / 2 - 12, LV_PART_MAIN);
		lv_obj_set_style_text_font(lblSpeed, get_montserrat_number_bold_48(), LV_STATE_DEFAULT);
		lv_obj_set_style_text_align(lblSpeed, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
		lv_obj_align(lblSpeed, LV_ALIGN_TOP_RIGHT, -12, 15);

		lv_obj_set_style_width(lblSpeedUnit, SCREEN_WIDTH / 2 - 12, LV_PART_MAIN);
		lv_obj_set_style_text_font(lblSpeedUnit, get_montserrat_24(), LV_STATE_DEFAULT);
		lv_obj_set_style_text_align(lblSpeedUnit, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
		lv_obj_align(lblSpeedUnit, LV_ALIGN_TOP_RIGHT, -12, 50);

		lv_label_set_long_mode(lblDistanceToNextRoad, LV_LABEL_LONG_SCROLL_CIRCULAR);
		lv_obj_set_style_width(lblDistanceToNextRoad, SCREEN_WIDTH, LV_PART_MAIN);
		lv_obj_set_style_text_font(lblDistanceToNextRoad, get_montserrat_semibold_28(), LV_STATE_DEFAULT);
		lv_obj_set_style_text_align(lblDistanceToNextRoad, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
		lv_obj_align(lblDistanceToNextRoad, LV_ALIGN_TOP_MID, 0, 85);

		lv_label_set_long_mode(lblNextRoad, LV_LABEL_LONG_WRAP);
		lv_obj_set_style_width(lblNextRoad, SCREEN_WIDTH, LV_PART_MAIN);
		lv_obj_set_style_text_font(lblNextRoad, get_montserrat_semibold_28(), LV_STATE_DEFAULT);
		lv_obj_set_style_text_align(lblNextRoad, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
		lv_obj_align_to(lblNextRoad, lblDistanceToNextRoad, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

		lv_label_set_long_mode(lblNextRoadDesc, LV_LABEL_LONG_WRAP);
		lv_obj_set_style_width(lblNextRoadDesc, SCREEN_WIDTH, LV_PART_MAIN);
		lv_obj_set_style_text_font(lblNextRoadDesc, get_montserrat_semibold_24(), LV_STATE_DEFAULT);
		lv_obj_set_style_text_align(lblNextRoadDesc, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
		// FIXME: align not working for wrapped text when height changes
		lv_obj_align_to(lblNextRoadDesc, lblNextRoad, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

		lv_label_set_long_mode(lblEta, LV_LABEL_LONG_SCROLL_CIRCULAR);
		lv_obj_set_style_text_font(lblEta, get_montserrat_24(), LV_STATE_DEFAULT);
		lv_obj_set_style_width(lblEta, SCREEN_WIDTH, LV_PART_MAIN);
		lv_obj_align(lblEta, LV_ALIGN_BOTTOM_MID, 0, -5);
#endif
	}

	void update() {
		using namespace details;
		if (millis() - details::lastUpdate < 5)
			return;

		details::lastUpdate = millis();
		lv_timer_handler();

		if (Data::details::iconDirty) {
			Data::details::iconDirty = false;

			static lv_img_dsc_t icon;
			icon.header.cf     = LV_COLOR_FORMAT_RGB565;
			icon.header.w      = ICON_WIDTH;
			icon.header.h      = ICON_HEIGHT;
			icon.header.stride = ICON_WIDTH * (LV_COLOR_DEPTH) / 8;
			icon.data_size     = ICON_RENDER_BUFFER_SIZE;
			icon.data          = (const uint8_t*)&Data::details::iconRenderBuffer;
			lv_img_set_src(imgTbtIcon, &icon);
		}
	}
} // namespace UI

void convert1BitBitmapToRgb565(void* dst, const void* src, uint16_t width, uint16_t height, uint16_t color, uint16_t bgColor, bool invert = false) {
	uint16_t* d      = (uint16_t*)dst;
	const uint8_t* s = (const uint8_t*)src;

	auto activeColor   = invert ? bgColor : color;
	auto inactiveColor = invert ? color : bgColor;

	for (uint16_t y = 0; y < height; y++) {
		for (uint16_t x = 0; x < width; x++) {
			if (s[(y * width + x) / 8] & (1 << (7 - x % 8))) {
				d[y * width + x] = activeColor;
			} else {
				d[y * width + x] = inactiveColor;
			}
		}
	}
}

namespace Data {

	bool hasNavigationData();
	bool hasSpeedData();
	void clearNavigationData();
	void clearSpeedData();
	int speed();
	void setSpeed(const int& value);
	String nextRoad();
	void setNextRoad(const String& value);
	String nextRoadDesc();
	void setNextRoadDesc(const String& value);
	String eta();
	void setEta(const String& value);
	String ete();
	void setEte(const String& value);
	String totalDistance();
	void setTotalDistance(const String& value);
	String distanceToNextTurn();
	void setDistanceToNextTurn(const String& value);
	String displayIconHash();
	void setIconHash(const String& value);
	uint8_t* iconRenderBuffer();
	void setIconBuffer(const uint8_t* value, const size_t& length);
	String fullEta();
	void saveIcon(const String& iconHash, const uint8_t* buffer);
	bool isIconExisted(const String& iconHash);
	void loadIcon(const String& iconHash);
	void receiveNewIcon(const String& iconHash, const uint8_t* buffer);

	void removeAllFiles();
	void listFiles();
	size_t readFile(const String& filename, uint8_t* buffer, const size_t bufferSize);
	void writeFile(const String& filename, const uint8_t* buffer, const size_t& length);

	void init() {
		if (!FS.begin(FORMAT_SPIFFS_IF_FAILED)) {
			Serial.println("Error mounting SPIFFS");
			return;
		}

		listFiles();
	}

	bool hasNavigationData() {
		return !(details::nextRoad.isEmpty() && details::nextRoadDesc.isEmpty() && details::eta.isEmpty() &&
		         details::distanceToNextTurn.isEmpty());
	}

	bool hasSpeedData() {
		return details::speed >= 0;
	}

	void clearNavigationData() {
		setNextRoad(String());
		setNextRoadDesc(String());
		setEta(String());
		setEte(String());
		setDistanceToNextTurn(String());
		setTotalDistance(String());
		setIconHash(String());
		details::receivedIconHash = String();
	}

	void clearSpeedData() {
		setSpeed(-1);
	}

	int speed() {
		return std::max(details::speed, 0);
	}

	void setSpeed(const int& value) {
		if (value == details::speed)
			return;

		details::speed = value;

		if (value == -1) {
			lv_label_set_text(UI::details::lblSpeed, "");
		} else {
			lv_label_set_text(UI::details::lblSpeed, String(value).c_str());
		}
	}

	String nextRoad() {
		return hasNavigationData() ? details::nextRoad : "---";
	}

	void setNextRoad(const String& value) {
		if (value == details::nextRoad)
			return;

		if (!value.isEmpty() && value != details::nextRoad) {
			ThemeControl::flashScreen();
		}
		details::nextRoad = value;

		lv_label_set_text(UI::details::lblNextRoad, value.c_str());
	}

	String nextRoadDesc() {
		return hasNavigationData() ? details::nextRoadDesc : "---";
	}

	void setNextRoadDesc(const String& value) {
		if (value == details::nextRoadDesc)
			return;

		details::nextRoadDesc = value;

		lv_label_set_text(UI::details::lblNextRoadDesc, value.c_str());
	}

	String eta() {
		return hasNavigationData() ? details::eta : "---";
	}

	void setEta(const String& value) {
		if (value == details::eta)
			return;

		details::eta = value;

		lv_label_set_text(UI::details::lblEta, fullEta().c_str());
	}

	String ete() {
		return hasNavigationData() ? details::ete : "---";
	}

	void setEte(const String& value) {
		if (value == details::ete)
			return;
		details::ete = value;

		lv_label_set_text(UI::details::lblEta, fullEta().c_str());
	}

	String totalDistance() {
		return hasNavigationData() ? details::totalDistance : "---";
	}

	void setTotalDistance(const String& value) {
		if (value == details::totalDistance)
			return;
		details::totalDistance = value;

		lv_label_set_text(UI::details::lblEta, fullEta().c_str());
	}

	String distanceToNextTurn() {
		return hasNavigationData() ? details::distanceToNextTurn : "---";
	}

	void setDistanceToNextTurn(const String& value) {
		if (value == details::distanceToNextTurn)
			return;
		details::distanceToNextTurn = value;

		lv_label_set_text(UI::details::lblDistanceToNextRoad, value.c_str());
	}

	String fullEta() {
		return ete() + " - " + totalDistance() + " - " + eta();
	}

	String displayIconHash() {
		return details::displayIconHash;
	}

	void setIconHash(const String& value) {
		if (value == details::displayIconHash)
			return;

		details::displayIconHash = value;

		Serial.println("Icon hash changed: " + value);

		if (value.isEmpty()) {
			setIconBuffer(nullptr, 0);
			return;
		}

		if (isIconExisted(value)) {
			Serial.println("Icon already existed, now display");
			loadIcon(value);
			return;
		}

		// Serial.println("Requesting icon");
		// Request icon
		// notifyCharacteristic(CHA_NAV_TBT_ICON, (uint8_t*)value.c_str(), value.length());
	}

	uint8_t* iconRenderBuffer() {
		return details::iconRenderBuffer;
	}

	void setIconBuffer(const uint8_t* value, const size_t& length) {
		// Blank icon
		if (!value || length == 0) {
			memset(details::iconRenderBuffer, 0xFF, sizeof(details::iconRenderBuffer));
			details::iconDirty = true;
			return;
		}

		// Render icon
		if (length > sizeof(details::iconRenderBuffer) / LV_COLOR_DEPTH) {
			Serial.println("Error: Icon buffer overflow");
		} else {
			Serial.println("Drawing icon");
			convert1BitBitmapToRgb565(details::iconRenderBuffer, value, 64, 64, lv_color_to_u16(lv_color_make(0x00, 0x00, 0xFF)),
			                          lv_color_to_u16(lv_color_make(0xFF, 0xFF, 0xFF)));
			details::iconDirty = true;
		}
	}

	void removeAllFiles() {
		File root = FS.open("/");
		File file = root.openNextFile();

		while (file) {
			Serial.print("Removing file: ");
			Serial.println(file.path());
			FS.remove(file.path());
			file = root.openNextFile();
		}
	}

	void listFiles() {
		Serial.println("Listing files");
		File root = FS.open("/");
		File file = root.openNextFile();

		details::availableIcons.clear();

		while (file) {
			String name = file.name();
			String hash = name.substring(0, name.length() - 4);
			Serial.print("File: ");
			Serial.print(name);
			Serial.print(" Hash: ");
			Serial.print(hash);
			Serial.print(" Size: ");
			Serial.println(file.size());
			// Remove extension
			details::availableIcons.push_back(hash);
			file = root.openNextFile();
		}
	}

	size_t readFile(const String& filename, uint8_t* buffer, const size_t bufferSize) {
		Serial.println("Reading file: " + filename);
		File file = FS.open(filename, FILE_READ);

		if (!file && !file.isDirectory()) {
			Serial.println("Failed to open file for reading");
			return 0;
		}

		if (file.size() > bufferSize) {
			Serial.println("Error: Buffer overflow");
			return 0;
		}

		size_t length = file.read(buffer, bufferSize);
		file.close();

		return length;
	}

	void writeFile(const String& filename, const uint8_t* buffer, const size_t& length) {
		Serial.println("Writing file: " + filename + " size: " + length);
		File file = FS.open(filename, FILE_WRITE);

		if (!file) {
			Serial.println("Failed to open file for writing");
			return;
		}

		file.write(buffer, length);
		file.close();
	}

	bool isIconExisted(const String& iconHash) {
		return std::find(details::availableIcons.begin(), details::availableIcons.end(), iconHash) !=
		       details::availableIcons.end();
	}

	void saveIcon(const String& iconHash, const uint8_t* buffer) {
		if (isIconExisted(iconHash)) {
			Serial.println("Icon existed");
			return;
		}

		writeFile(String("/") + iconHash + ".bin", buffer, ICON_BITMAP_BUFFER_SIZE);
		details::availableIcons.push_back(iconHash);

		Serial.println(String("Icon saved: ") + iconHash + ", total: " + details::availableIcons.size());
	}

	void loadIcon(const String& iconHash) {
		if (!isIconExisted(iconHash)) {
			Serial.println("Icon not found");
			return;
		}

		readFile(String("/") + iconHash + ".bin", details::iconBitmapBuffer, ICON_BITMAP_BUFFER_SIZE);
		setIconBuffer(details::iconBitmapBuffer, ICON_BITMAP_BUFFER_SIZE);
	}

	void receiveNewIcon(const String& iconHash, const uint8_t* buffer) {
		if (iconHash == details::receivedIconHash) {
			Serial.println("Icon already received");
			return;
		}

		details::receivedIconHash = iconHash;
		memcpy(details::receivedIconBitmapBuffer, buffer, ICON_BITMAP_BUFFER_SIZE);
	}

	void update() {
		if (details::receivedIconHash.isEmpty())
			return;

		// Save icon for later use
		if (!isIconExisted(details::receivedIconHash)) {
			saveIcon(details::receivedIconHash, details::receivedIconBitmapBuffer);
		}

		// Display icon
		if (details::receivedIconHash == displayIconHash()) {
			setIconBuffer(details::receivedIconBitmapBuffer, ICON_BITMAP_BUFFER_SIZE);
		}

		details::receivedIconHash = String();
	}
} // namespace Data


#endif // UI_H