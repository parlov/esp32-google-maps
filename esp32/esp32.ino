#include "ble.h"
#include "config.h"
#include "keyval.h"
#include "preferences.h"
#include "scheduler.h"
#include "theme.h"
#include "ui.h"

#include <queue>

std::queue<String> navigationQueue{};
bool connectionChanged = true;
bool oldIsOverspeed    = false;

void onCharacteristicWrite(const String& uuid, uint8_t* data, size_t length) {
	String value = (uuid != CHA_NAV_TBT_ICON) ? String((char*)(data)) : String();

	if (uuid == CHA_SETTINGS) {
		const auto kv = kvParseMultiline(value);

		Pref::lightTheme = kv.getOrDefault("lightTheme", "false") == "true";
		Pref::brightness = kv.getOrDefault("brightness", "100").toInt();
		Pref::speedLimit = kv.getOrDefault("speedLimit", "60").toInt();

		lcd.setBrightness(Pref::brightness);
		Pref::lightTheme ? ThemeControl::light() : ThemeControl::dark();

		if (kv.contains("removeAllFiles")) {
			Data::removeAllFiles();
		}
	}

	if (uuid == CHA_NAV) {
		navigationQueue.push(value);
		pongNavigation();
	}

	if (uuid == CHA_NAV_TBT_ICON) {
		int semicolonIndex = -1;
		for (uint8_t i = 0; i < 16; i++) {
			if (data[i] == ';') {
				semicolonIndex = i;
				break;
			}
		}

		if (semicolonIndex <= 0) {
			Serial.println("No hash found");
			return;
		}

		String iconHash = String((char*)data, semicolonIndex);
		auto iconSize   = length - semicolonIndex - 1;
		Serial.println(String("Received icon w/ hash: ") + iconHash + " size: " + iconSize);

		if (iconSize != ICON_BITMAP_BUFFER_SIZE) {
			Serial.println("Invalid icon size");
			return;
		}

		Data::receiveNewIcon(iconHash, data + semicolonIndex + 1);

		pongNavigation();
	}

	if (uuid == CHA_GPS_SPEED) {
		navigationQueue.push(String("speed=") + value);

		pongSpeed();
	}
}

void onConnectionChange(bool connected) {
	connectionChanged = true;
}

uint32_t gLastNavigationDataReceived_ms = 0;
uint32_t gLastSpeedDataReceived_ms      = 0;

void pongNavigation() {
	gLastNavigationDataReceived_ms = millis();
}

void pongSpeed() {
	gLastSpeedDataReceived_ms = millis();
}

void processQueue() {
	if (navigationQueue.empty())
		return;

	const auto& data = navigationQueue.front();
	const auto kv    = kvParseMultiline(data);

	if (kv.contains("nextRd")) {
		Data::setNextRoad(kv.getOrDefault("nextRd"));
	}
	if (kv.contains("nextRdDesc")) {
		Data::setNextRoadDesc(kv.getOrDefault("nextRdDesc"));
	}
	if (kv.contains("distToNext")) {
		Data::setDistanceToNextTurn(kv.getOrDefault("distToNext"));
	}
	if (kv.contains("totalDist")) {
		Data::setTotalDistance(kv.getOrDefault("totalDist"));
	}
	if (kv.contains("eta")) {
		Data::setEta(kv.getOrDefault("eta"));
	}
	if (kv.contains("ete")) {
		Data::setEte(kv.getOrDefault("ete"));
	}
	if (kv.contains("iconHash")) {
		Data::setIconHash(kv.getOrDefault("iconHash"));
	}
	if (kv.contains("speed")) {
		Data::setSpeed(kv.getOrDefault("speed").toInt());
	}

	navigationQueue.pop();
}

void setup() {
	delay(2000);

	Serial.begin(115200);
	Serial.println("Initializing BLE...");
	initBle();

	Serial.println("Initializing UI...");
	UI::init();

	Data::init();

	lcd.setBrightness(Pref::brightness);
	ThemeControl::dark();

	Serial.println("Init done");
}

bool isOverspeed(int speed) {
	return speed >= Pref::speedLimit;
}

void loop() {
	UI::update();
	ThemeControl::update();
	Data::update();

	processQueue();

	// Overspeed check
	const auto newIsOverspeed = isOverspeed(Data::speed());
	if (newIsOverspeed != oldIsOverspeed) {
		oldIsOverspeed = newIsOverspeed;

		if (newIsOverspeed)
			ThemeControl::flashScreen();
	}

	DO_EVERY(10000) {
		if (isOverspeed(Data::speed())) {
			ThemeControl::flashScreen();
		}
	}

	// Connection status
	if (connectionChanged) {
		connectionChanged = false;

		if (deviceConnected) {
			Data::setNextRoad("PikiPuka-Ta ready!");
			Data::setNextRoadDesc("Connected!");
		} else {
			navigationQueue = std::queue<String>();
			Data::clearNavigationData();
			Data::clearSpeedData();
			Data::setNextRoadDesc("Disconnected!");
		}
	}
}
