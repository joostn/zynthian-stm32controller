
#include <USBComposite.h>

// For linear taper potmeter set to false, for logarithmic potmeter
// (called 'B taper') set to true:
const bool s_bcurve = false;
// knee of the correction curve (only used if s_bcurve == true):
const int s_bcurve_knee_src = 640;
const int s_bcurve_knee_dest = 2048;

// HID report descriptor: just a simple array of 32 bytes
// The report descriptor is not actually used by the client, it will just read raw bytes and interpret them by itself.
static const uint8_t s_report_descriptor[] = {
    0x06, 0x00, 0xFF,      /* USAGE_PAGE (Vendor Defined Page 1) */
    0x09, 0x01,            /* USAGE (Vendor Usage 1) */
    0xA1, 0x01,            /*  Collection (Application) */ \
    0x15, 0x00,    /* LOGICAL_MINIMUM (0) */ 
    0x26, 0xff, 0x00, /* LOGICAL_MAXIMUM (255) */
    0x75, 0x08,       /* REPORT_SIZE (8) */
    0x95, 32,       /* REPORT_COUNT */
    0x81, 0x02,     /* INPUT (Data,Var,Abs) */
    0xC0,          /*  end collection */ 
  };

// buffer for sending HID reports:
uint8_t s_HidBuf[32];

HIDReporter s_hidreporter(s_HidBuf, sizeof(s_HidBuf));

// This is not a valid allocated USB vendor id, but we'll hijack it for this purpose:
static uint16_t s_UsbVendorId = 0xdead;
static uint16_t s_UsbProductId = 0xf001;

void setup() {
  USBHID.begin(s_report_descriptor, (uint16_t) sizeof(s_report_descriptor),(uint16_t) s_UsbVendorId, (uint16_t) s_UsbProductId,
        "Zynthian", "STM32 controller", "1.0");
  delay(1000);
}

static int s_prevadcvalue = 0;
static int s_hyst = 0;
static int s_prevsendval = 0;
static int s_updatecounter = 0;

void loop() {
  int adcval = analogRead(0);   // 0..4095

  if(s_bcurve)
  {
    // convert adc readout from log to linear:
    // http://www.resistorguide.com/potentiometer-taper/
    if(adcval < s_bcurve_knee_src)
    {
      adcval = (s_bcurve_knee_dest * adcval) / s_bcurve_knee_src;
    }
    else
    {
      adcval = s_bcurve_knee_dest + ((4095-s_bcurve_knee_dest) * (adcval - s_bcurve_knee_src)) / (4095 - s_bcurve_knee_src);      
    }
  }

  // apply hysteresis (counteract the change in adc value by subtracting between 0 and 16):
  int delta = adcval - s_prevadcvalue;
  s_prevadcvalue = adcval;
  s_hyst -= delta;
  if(s_hyst < -16) s_hyst = -16;
  if(s_hyst > 0) s_hyst = 0;
  int correctedadcval = adcval + s_hyst;

  // apply dead zone at both ends:
  correctedadcval -= 8;

  // hysteresis and dead zone reduce the full scale. Scale and clamp to [0..4095]:
  correctedadcval += correctedadcval >> 7;
  if(correctedadcval < 0) correctedadcval = 0;
  if(correctedadcval > 4095) correctedadcval = 4095;

  if(correctedadcval != s_prevsendval)
  {
    // force sending an update if the value has changed:
    s_updatecounter = 0;
  }

  // by default only send an update every 16th iteration:
  if(s_updatecounter == 0)
  {
    memset(s_HidBuf, 0, sizeof(s_HidBuf));
    s_HidBuf[0] = (uint8_t)1;  // version number, for future use  
    s_HidBuf[1] = (uint8_t)0;  // version number, for future use
    s_HidBuf[2] = (uint8_t)correctedadcval; // low byte
    s_HidBuf[3] = (uint8_t)(correctedadcval >> 8); // high byte
    s_hidreporter.sendReport();
    s_prevsendval = correctedadcval;
  }
  s_updatecounter++;
  if(s_updatecounter >= 16)
  {
    s_updatecounter = 0;
  }
  delay(10); // 100 times per second
}
