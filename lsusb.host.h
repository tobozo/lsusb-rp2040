/*
 * The MIT License (MIT)
 *
 * Copyleft (c) 2023 tobozo
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#pragma once

#define LANGUAGE_ID 0x0409  // English

// #include "pico/stdlib.h"
// #include "pico/multicore.h"
// #include "pico/bootrom.h"


#include "pio_usb.h"          // Bring USB-HOST Headers from Library: Pico_PIO_USB
#include "tusb.h"             // tinyUSB stack
#include "Adafruit_TinyUSB.h" // Adafruit layer

// string functions, labels, helpers
#include "usb.org/lsusb_info.h"
#include "misc/helpers.h"


// Each HID instance can has multiple reports
#define MAX_REPORT  4
static struct
{
  uint8_t report_count;
  tuh_hid_report_info_t report_info[MAX_REPORT];
}hid_info[CFG_TUH_HID];

tusb_desc_device_t plugged_device;

void print_device_descriptor(tuh_xfer_t* xfer);
void parse_config_descriptor(uint8_t dev_addr, tusb_desc_configuration_t const* desc_cfg);
void parse_hid_interface(uint8_t daddr, tusb_desc_interface_t const *desc_itf, uint16_t max_len);
void parse_audio_interface(uint8_t daddr, tusb_desc_interface_t const *desc_itf, uint16_t max_len);
void parse_generic_interface(uint8_t daddr, tusb_desc_interface_t const *desc_itf, uint16_t max_len);


void debug_print(char* str)
{
  printf(str);
}


void tuh_mount_cb (uint8_t daddr)
{
  printf("[tuh_mount_cb] Device attached, address = %d\r\n", daddr);
  tuh_descriptor_get_device(daddr, &plugged_device, 18, print_device_descriptor, 0);
}


void tuh_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  printf("[tuh_umount_cb][%u] Interface%u is unmounted\r\n", dev_addr, instance);
}


void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
  printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);

  // Interface protocol (hid_interface_protocol_enum_t)
  const char* protocol_str[] = { "None", "Keyboard", "Mouse" };
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  printf("HID Interface Protocol = %s\r\n", protocol_str[itf_protocol]);

  // By default host stack will use activate boot protocol on supported interface.
  // Therefore for this simple example, we only need to parse generic report descriptor (with built-in parser)
  if ( itf_protocol == HID_ITF_PROTOCOL_NONE )
  {
    hid_info[instance].report_count = tuh_hid_parse_report_descriptor(hid_info[instance].report_info, MAX_REPORT, desc_report, desc_len);
    printf("HID has %u reports \r\n", hid_info[instance].report_count);
  }

  // request to receive report
  // tuh_hid_report_received_cb() will be invoked when report is available
  if ( !tuh_hid_receive_report(dev_addr, instance) )
  {
    printf("Error: cannot request to receive report\r\n");
  }
}


// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  printf("[tuh_hid_umount_cb][%u] HID Interface%u is unmounted\r\n", dev_addr, instance);
  free_hid_buf(dev_addr);
}


void hid_report_received(tuh_xfer_t* xfer)
{
  // Note: not all field in xfer is available for use (i.e filled by tinyusb stack) in callback to save sram
  // For instance, xfer->buffer is NULL. We have used user_data to store buffer when submitted callback
  uint8_t* buf = (uint8_t*) xfer->user_data;
  if (xfer->result == XFER_RESULT_SUCCESS) {
    printf("[dev %u: ep %02x] HID Report:", xfer->daddr, xfer->ep_addr);
    for(uint32_t i=0; i<xfer->actual_len; i++) {
      if (i%16 == 0) printf("\r\n  ");
      printf("%02X ", buf[i]);
    }
    printf("\r\n");
  } else {
    printf("Error\n");
  }
  // continue to submit transfer, with updated buffer
  // other field remain the same
  xfer->buflen = 64;
  xfer->buffer = buf;
  tuh_edpt_xfer(xfer);
}


uint16_t count_interface_total_len(tusb_desc_interface_t const* desc_itf, uint8_t itf_count, uint16_t max_len)
{
  uint8_t const* p_desc = (uint8_t const*) desc_itf;
  uint16_t len = 0;

  while (itf_count--) {
    // Next on interface desc
    len += tu_desc_len(desc_itf);
    p_desc = tu_desc_next(p_desc);
    while (len < max_len) {
      // return on IAD regardless of itf count
      if ( tu_desc_type(p_desc) == TUSB_DESC_INTERFACE_ASSOCIATION ) return len;
      if ( (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE) && ((tusb_desc_interface_t const*) p_desc)->bAlternateSetting == 0 ) {
        break;
      }
      len += tu_desc_len(p_desc);
      p_desc = tu_desc_next(p_desc);
    }
  }
  return len;
}


void print_device_descriptor(tuh_xfer_t* xfer)
{
  if ( XFER_RESULT_SUCCESS != xfer->result ) {
    printf("Failed to get device descriptor\r\n");
    return;
  }
  uint8_t const daddr = xfer->daddr;

  auto vid_pid = get_vid_pid( plugged_device.idVendor, plugged_device.idProduct );
  auto vendor  = vid_pid.vendor;
  auto product = vid_pid.product;
  auto class_sub_proto = get_class_sub_proto(plugged_device.bDeviceClass, plugged_device.bDeviceSubClass, plugged_device.bDeviceProtocol );

  printf("Device %u: ID %04x:%04x\r\n", daddr, plugged_device.idVendor, plugged_device.idProduct);
  printf("Device Descriptor:\r\n");
  printf("  bLength             %u\r\n"        , plugged_device.bLength);
  printf("  bDescriptorType     %u\r\n"        , plugged_device.bDescriptorType);
  printf("  bcdUSB              %04x\r\n"      , plugged_device.bcdUSB);
  printf("  bDeviceClass        %u %s\r\n"     , plugged_device.bDeviceClass, class_sub_proto.dev_class->name );
  printf("  bDeviceSubClass     %u %s\r\n"     , plugged_device.bDeviceSubClass, class_sub_proto.dev_subclass->name );
  printf("  bDeviceProtocol     %u %s\r\n"     , plugged_device.bDeviceProtocol, class_sub_proto.dev_proto->name);
  printf("  bMaxPacketSize0     %u\r\n"        , plugged_device.bMaxPacketSize0);
  printf("  idVendor            0x%04x %s\r\n" , plugged_device.idVendor, vendor->name );
  printf("  idProduct           0x%04x %s\r\n" , plugged_device.idProduct, product->name);
  printf("  bcdDevice           %04x\r\n"      , plugged_device.bcdDevice);
  // Get String descriptor using Sync API
  uint16_t temp_buf[128];
  printf("  iManufacturer       %u ", plugged_device.iManufacturer);
  if (XFER_RESULT_SUCCESS == tuh_descriptor_get_manufacturer_string_sync(daddr, LANGUAGE_ID, temp_buf, sizeof(temp_buf)) ) {
    print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf), debug_print);
  }
  printf("\r\n");
  printf("  iProduct            %u ", plugged_device.iProduct);
  if (XFER_RESULT_SUCCESS == tuh_descriptor_get_product_string_sync(daddr, LANGUAGE_ID, temp_buf, sizeof(temp_buf))) {
    print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf), debug_print);
  }
  printf("\r\n");
  printf("  iSerialNumber       %u ", plugged_device.iSerialNumber);
  if (XFER_RESULT_SUCCESS == tuh_descriptor_get_serial_string_sync(daddr, LANGUAGE_ID, temp_buf, sizeof(temp_buf))) {
    print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf), debug_print);
  }
  printf("\r\n");
  printf("  bNumConfigurations  %u\r\n", plugged_device.bNumConfigurations);
  // Get configuration descriptor with sync API
  if (XFER_RESULT_SUCCESS == tuh_descriptor_get_configuration_sync(daddr, 0, temp_buf, sizeof(temp_buf))) {
    parse_config_descriptor(daddr, (tusb_desc_configuration_t*) temp_buf);
  }
}


// simple configuration parser
void parse_config_descriptor(uint8_t dev_addr, tusb_desc_configuration_t const* desc_cfg)
{
  uint8_t const* desc_end = ((uint8_t const*) desc_cfg) + tu_le16toh(desc_cfg->wTotalLength);
  uint8_t const* p_desc   = tu_desc_next(desc_cfg);

  printf("  Configuration Descriptor:\n");
  printf("    bLength             %8d\n",        desc_cfg->bLength );
  printf("    bDescriptorType     %8d\n",        desc_cfg->bDescriptorType );
  printf("    wTotalLength          0x%04x\n",   desc_cfg->wTotalLength );
  printf("    bNumInterfaces      %8d\n",        desc_cfg->bNumInterfaces );
  printf("    bConfigurationValue %8d\n",        desc_cfg->bConfigurationValue );
  printf("    iConfiguration      %8d\n",        desc_cfg->iConfiguration );
  printf("    bmAttributes            0x%02x\n", desc_cfg->bmAttributes );

  // parse each interfaces
  while( p_desc < desc_end ) {
    uint8_t assoc_itf_count = 1;
    // Class will always starts with Interface Association (if any) and then Interface descriptor
    if ( TUSB_DESC_INTERFACE_ASSOCIATION == tu_desc_type(p_desc) ) {
      auto desc_iad = (tusb_desc_interface_assoc_t const *) p_desc;
      assoc_itf_count = desc_iad->bInterfaceCount;
      p_desc = tu_desc_next(p_desc); // next to Interface
    }

    // must be interface from now
    if( TUSB_DESC_INTERFACE != tu_desc_type(p_desc) ) return;
    auto desc_itf = (tusb_desc_interface_t const*) p_desc;
    auto class_sub_proto = get_class_sub_proto(desc_itf->bInterfaceClass, desc_itf->bInterfaceSubClass, desc_itf->bInterfaceProtocol );
    uint16_t const drv_len = count_interface_total_len(desc_itf, assoc_itf_count, (uint16_t) (desc_end-p_desc));

    printf("    Interface Descriptor #%d:\n",   desc_itf->bInterfaceNumber );
    printf("      bLength            %8d\n",    desc_itf->bLength );
    printf("      bDescriptorType    %8d\n",    desc_itf->bDescriptorType );
    printf("      bInterfaceNumber   %8d\n",    desc_itf->bInterfaceNumber );
    printf("      bAlternateSetting  %8d\n",    desc_itf->bAlternateSetting );
    printf("      bNumEndpoints      %8d\n",    desc_itf->bNumEndpoints );
    printf("      bInterfaceClass    %8d %s\n", desc_itf->bInterfaceClass, class_sub_proto.dev_class->name );
    printf("      bInterfaceSubClass %8d %s\n", desc_itf->bInterfaceSubClass, class_sub_proto.dev_subclass->name  );
    printf("      bInterfaceProtocol %8d %s\n", desc_itf->bInterfaceProtocol, class_sub_proto.dev_proto->name );
    printf("      iInterface         %8d\n",    desc_itf->iInterface );

    if(drv_len < sizeof(tusb_desc_interface_t)) { // probably corrupted descriptor
      printf("      ***CORRUPTED DESCRIPTOR\n");
      return;
    }

    switch( desc_itf->bInterfaceClass ) { // type = tusb_class_code_t
      case TUSB_CLASS_HID                  /*3   */: parse_hid_interface(dev_addr, desc_itf, drv_len); break;
      case TUSB_CLASS_AUDIO                /*1   */: parse_audio_interface(dev_addr, desc_itf, drv_len ); break;
      default                                      : parse_generic_interface( dev_addr, desc_itf, drv_len ); break;
      // case TUSB_CLASS_UNSPECIFIED          /*0   */: printf("[IGNORED] Unspecified Class\n"); break;
      // case TUSB_CLASS_CDC                  /*2   */: printf("[IGNORED] CDC Class\n"); break;
      //
      // case TUSB_CLASS_RESERVED_4           /*4   */: printf("[IGNORED] Reserved class\n"); break;
      // case TUSB_CLASS_PHYSICAL             /*5   */: printf("[IGNORED] PHY class\n"); break;
      // case TUSB_CLASS_IMAGE                /*6   */: printf("[IGNORED] Imaging class\n"); break;
      // case TUSB_CLASS_PRINTER              /*7   */: printf("[IGNORED] Printer class\n"); break;
      // case TUSB_CLASS_MSC                  /*8   */: printf("[IGNORED] Mass Storage class\n"); /*parse_endpoint_descriptors(dev_addr, desc_itf, drv_len);*/ break;
      // case TUSB_CLASS_HUB                  /*9   */: printf("[IGNORED] HUB class\n"); break;
      // case TUSB_CLASS_CDC_DATA             /*10  */: printf("[IGNORED] CDC Data class\n"); break;
      // case TUSB_CLASS_SMART_CARD           /*11  */: printf("[IGNORED] SmartCard class\n"); break;
      // case TUSB_CLASS_RESERVED_12          /*12  */: printf("[IGNORED] Reserved class\n"); break;
      // case TUSB_CLASS_CONTENT_SECURITY     /*13  */: printf("[IGNORED] Content Security class\n"); break;
      // case TUSB_CLASS_VIDEO                /*14  */: printf("[IGNORED] Video class\n"); break;
      // case TUSB_CLASS_PERSONAL_HEALTHCARE  /*15  */: printf("[IGNORED] Health Sensor class\n"); break;
      // case TUSB_CLASS_AUDIO_VIDEO          /*16  */: printf("[IGNORED] Audio+Video class\n"); break;
      //                                      /*    */
      // case TUSB_CLASS_DIAGNOSTIC           /*0xDC*/: printf("[IGNORED] Diagnostic class\n"); break;
      // case TUSB_CLASS_WIRELESS_CONTROLLER  /*0xE0*/: printf("[IGNORED] Wireless Controller class\n"); break;
      // case TUSB_CLASS_MISC                 /*0xEF*/: printf("[IGNORED] Misc class\n"); break;
      // case TUSB_CLASS_APPLICATION_SPECIFIC /*0xFE*/: printf("[IGNORED] App Specific class\n"); break;
      // case TUSB_CLASS_VENDOR_SPECIFIC      /*0xFF*/: printf("[IGNORED] Vendor Specific class\n"); break;
    }
    // next Interface or IAD descriptor
    p_desc += drv_len;
  }
}


void parse_audio_interface(uint8_t daddr, tusb_desc_interface_t const *desc_itf, uint16_t max_len)
{
  (void)daddr;
  (void)max_len;
  uint8_t const *p_desc = (uint8_t const *) desc_itf;
  // get first descriptor
  p_desc = tu_desc_next(p_desc);

  switch( desc_itf->bInterfaceSubClass ) {
    case AUDIO_SUBCLASS_CONTROL: {
      auto ac = (audio_desc_cs_ac_interface_t*) p_desc;
      printf("      AudioControl Interface Descriptor (control)\n");
      printf("        bLength           %3d\n",     ac->bLength ); ///< Size of this descriptor in bytes: 9.
      printf("        bDescriptorType    %2d\n",    ac->bDescriptorType ); ///< Descriptor Type. Value: TUSB_DESC_CS_INTERFACE.
      printf("        bDescriptorSubType %2d %s\n", ac->bDescriptorSubType, ac->bDescriptorSubType==1?"(HEADER)":"" ); ///< Descriptor SubType.
      printf("        bcdADC            %3d\n",     ac->bcdADC ); ///< Audio Device Class Specification Release Number in Binary-Coded Decimal. Value: U16_TO_U8S_LE(0x0200).
      printf("        bCategory          %2d\n",    ac->bCategory ); ///< Constant, indicating the primary use of this audio function, as intended by the manufacturer.
      printf("        wTotalLength   0x%04x\n",     ac->wTotalLength ); ///< Total number of bytes returned for the class-specific descriptor
      printf("        bmControls         %2d\n",    ac->bmControls ); ///< See: audio_cs_ac_interface_control_pos_t.
    } break;
    case AUDIO_SUBCLASS_STREAMING: {
      auto as = (audio_desc_cs_as_interface_t*) p_desc;
      printf("      AudioControl Interface Descriptor (stream):\n");
      printf("        bLength            %3d\n",    as->bLength            ); ///< Size of this descriptor, in bytes: 16.
      printf("        bDescriptorType    %2d\n",    as->bDescriptorType    ); ///< Descriptor Type. Value: TUSB_DESC_CS_INTERFACE.
      printf("        bDescriptorSubType %2d %s\n", as->bDescriptorSubType, as->bDescriptorSubType==1?"(HEADER)":"" ); ///< Descriptor SubType.
      printf("        bTerminalLink      %2d\n",    as->bTerminalLink      ); ///< The Terminal ID of the Terminal to which this interface is connected.
      printf("        bmControls         %2d\n",    as->bmControls         ); ///< See: audio_cs_as_interface_control_pos_t.
      printf("        bFormatType        %2d\n",    as->bFormatType        ); ///< Constant identifying the Format Type the AudioStreaming interface is using.
      printf("        bmFormats          %2lu\n",    as->bmFormats          ); ///< The Audio Data Format(s) that can be used to communicate with this interface.
      printf("        bNrChannels        %2d\n",    as->bNrChannels        ); ///< Number of physical channels in the AS Interface audio channel cluster.
      printf("        bmChannelConfig    %2lu\n",    as->bmChannelConfig    ); ///< Describes the spatial location of the physical channels. See: audio_channel_config_t.
      printf("        iChannelNames      %2d\n",    as->iChannelNames      ); ///< Index of a string descriptor, describing the name of the first physical channel.
    } break;
    case AUDIO_SUBCLASS_MIDI_STREAMING: {
      auto midi_header = (midi_desc_header_t*) p_desc;
      auto descType = midi_header->bDescriptorType;
      if( midi_header->bDescriptorSubType != MIDI_CS_INTERFACE_HEADER ) return;
      int to_read = midi_header->wTotalLength - midi_header->bLength;
      printf("      MIDIStreaming Interface Descriptor (head):\n");
      printf("        bLength            %2d\n",      midi_header->bLength            ); ///< Size of this descriptor in bytes.
      printf("        bDescriptorType    %2d\n",      midi_header->bDescriptorType    ); ///< Descriptor Type, must be Class-Specific
      printf("        bDescriptorSubType %2d (%s)\n", midi_header->bDescriptorSubType, midi_desc_subtype_to_string(midi_header->bDescriptorSubType) );
      printf("        bcdMSC             %2d\n",      midi_header->bcdMSC             ); ///< MidiStreaming SubClass release number in Binary-Coded Decimal
      printf("        wTotalLength       %2d\n",      midi_header->wTotalLength       );

      do {
        p_desc = tu_desc_next(midi_header);
        midi_header = (midi_desc_header_t*) p_desc;
        if( descType == midi_header->bDescriptorType ) {
          switch( midi_header->bDescriptorSubType ) {
            case MIDI_CS_INTERFACE_IN_JACK: {
                auto midi_in_jack = (midi_desc_in_jack_t*)midi_header;
                to_read -= midi_in_jack->bLength;
                printf("      MIDIStreaming Interface Descriptor(IN):\n");
                printf("        bLength            %2d\n",      midi_in_jack->bLength            ); ///< Size of this descriptor in bytes.
                printf("        bDescriptorType    %2d\n",      midi_in_jack->bDescriptorType    ); ///< Descriptor Type, must be Class-Specific
                printf("        bDescriptorSubType %2d (%s)\n", midi_in_jack->bDescriptorSubType, midi_desc_subtype_to_string(midi_in_jack->bDescriptorSubType) );
                printf("        bJackType          %2d %s\n",   midi_in_jack->bJackType, bJackType_to_string(midi_in_jack->bJackType) );
                printf("        bJackID            %2d\n",      midi_in_jack->bJackID            ); ///< Unique ID for MIDI IN Jack
                printf("        iJack              %2d\n",      midi_in_jack->iJack              ); ///< string descriptor
            } break;
            case MIDI_CS_INTERFACE_OUT_JACK: {
                auto midi_out_jack = (midi_desc_out_jack_t*)midi_header;
                to_read -= midi_out_jack->bLength;
                printf("      MIDIStreaming Interface Descriptor(OUT):\n");
                printf("        bLength             %2d\n",      midi_out_jack->bLength            ); ///< Size of this descriptor in bytes.
                printf("        bDescriptorType     %2d\n",      midi_out_jack->bDescriptorType    ); ///< Descriptor Type, must be Class-Specific
                printf("        bDescriptorSubType  %2d (%s)\n", midi_out_jack->bDescriptorSubType, midi_desc_subtype_to_string(midi_out_jack->bDescriptorSubType) );
                printf("        bJackType           %2d %s\n",   midi_out_jack->bJackType, bJackType_to_string(midi_out_jack->bJackType) );
                printf("        bJackID             %2d\n",      midi_out_jack->bJackID            ); ///< Unique ID for MIDI IN Jack
                printf("        bNrInputPins        %2d\n",      midi_out_jack->bNrInputPins       );
                printf("        baSourceID          %2d\n",      midi_out_jack->baSourceID         );
                printf("        baSourcePin         %2d\n",      midi_out_jack->baSourcePin        );
                printf("        iJack               %2d\n",      midi_out_jack->iJack              ); ///< string descriptor
            } break;
            default:
            to_read = 0;
            printf("      [ERROR] Unhandled bDescriptorSubType: %d\n", midi_header->bDescriptorSubType );
            return;
            break;
          }
        } else {
          auto desc_ep = (tusb_desc_endpoint_t const *) p_desc;
          to_read -= desc_ep->bLength;

          if( desc_ep->bDescriptorType == 0x05 /*AUDIO_CS_AC_INTERFACE_SELECTOR_UNIT*/ ) {
            printf("      Endpoint Descriptor:\n");
            printf("        bLength         %8d\n",           desc_ep->bLength);
            printf("        bDescriptorType %8d\n",           desc_ep->bDescriptorType);
            printf("        bEndpointAddress    0x%02x %s\n", desc_ep->bEndpointAddress, bEndpointAddress_to_string(desc_ep->bEndpointAddress) );
            parse_bm_attributes( desc_ep, "      ");
            printf("        wMaxPacketSize    0x%04x\n",      desc_ep->wMaxPacketSize );
            printf("        bInterval       %8d\n",           desc_ep->bInterval );
          } else {
            bEndpointAddress_t edp = {desc_ep->bEndpointAddress};

            if( edp.bits.dir==1 ) {
              auto desc_in = (midi_desc_in_jack_t*)p_desc;
              printf("      MIDIStreaming Endpoint Descriptor:\n");
              printf("        bLength            %8d\n", desc_in->bLength);
              printf("        bDescriptorType    %8d\n", desc_in->bDescriptorType);
              printf("        bDescriptorSubType %8d\n", desc_in->bDescriptorSubType);
              printf("        bJackType          %8d\n", desc_in->bJackType);
              printf("        bJackID            %8d\n", desc_in->bJackID);
            } else {
              auto desc_out = (midi_desc_out_jack_t*)p_desc;
              printf("      MIDIStreaming Endpoint Descriptor:\n");
              printf("        bLength            %8d\n", desc_out->bLength);
              printf("        bDescriptorType    %8d\n", desc_out->bDescriptorType);
              printf("        bDescriptorSubType %8d\n", desc_out->bDescriptorSubType);
              printf("        bJackType          %8d\n", desc_out->bJackType);
              printf("        bJackID            %8d\n", desc_out->bJackID);
            }
          }
        }

      } while( to_read > 0 );
    } break; // end case AUDIO_SUBCLASS_MIDI_STREAMING
    default:
      printf("      [ERROR] Bad Interface SubClass: %d\n", desc_itf->bInterfaceSubClass );
    break;
  } // end switch( desc_itf->bInterfaceSubClass )
}


void parse_generic_interface(uint8_t daddr, tusb_desc_interface_t const *desc_itf, uint16_t max_len)
{
  (void)daddr;
  uint8_t const *p_desc = (uint8_t const *) desc_itf;
  auto desc_generic = (tusb_desc_endpoint_t const *) p_desc;
  uint16_t bytes_read = desc_generic->bLength;
  p_desc += desc_generic->bLength;

  do {
    desc_generic = (tusb_desc_endpoint_t const *) p_desc;
    if( desc_generic->bLength == 0 ) break;
    p_desc += desc_generic->bLength;
    bytes_read += desc_generic->bLength;
    printf("        Generic Endpoint Descriptor:\n");
    printf("          bLength         %2d\n", desc_generic->bLength);         /**< Numeric expression that is the total size of the HID descriptor */
    printf("          bDescriptorType %2d\n", desc_generic->bDescriptorType); /**< Constant name specifying type of HID descriptor. */
    printf("          bEndpointAddress    0x%02x %s\n", desc_generic->bEndpointAddress, bEndpointAddress_to_string(desc_generic->bEndpointAddress) );
    parse_bm_attributes( desc_generic, "        ");
    printf("          wMaxPacketSize    0x%04x\n",      desc_generic->wMaxPacketSize );
    printf("          bInterval       %8d\n",           desc_generic->bInterval );
  } while( bytes_read <=max_len );
}


void parse_hid_interface(uint8_t daddr, tusb_desc_interface_t const *desc_itf, uint16_t max_len)
{
  (void)daddr;
  // len = interface + hid + n*endpoints
  uint16_t const drv_len = (uint16_t) (sizeof(tusb_desc_interface_t) + sizeof(tusb_hid_descriptor_hid_t) + desc_itf->bNumEndpoints * sizeof(tusb_desc_endpoint_t));
  // corrupted descriptor
  if (max_len < drv_len) {
    printf("        [ERROR] Len overvlow\n");
    return;
  }
  uint8_t const *p_desc = (uint8_t const *) desc_itf;
  // HID descriptor
  p_desc = tu_desc_next(p_desc);
  auto desc_hid = (tusb_hid_descriptor_hid_t const *) p_desc;
  if(HID_DESC_TYPE_HID != desc_hid->bDescriptorType) {
    printf("        [ERROR] Not HID Type\n");
    return;
  }

  printf("        HID Device Descriptor:\n");
  printf("          bLength         %2d\n", desc_hid->bLength);         /**< Numeric expression that is the total size of the HID descriptor */
  printf("          bDescriptorType %2d\n", desc_hid->bDescriptorType); /**< Constant name specifying type of HID descriptor. */
  printf("          bcdHID         %3d\n",  desc_hid->bcdHID);          /**< Numeric expression identifying the HID Class Specification release */
  printf("          bCountryCode    %2d\n", desc_hid->bCountryCode);    /**< Numeric expression identifying country code of the localized hardware.  */
  printf("          bNumDescriptors %2d\n", desc_hid->bNumDescriptors); /**< Numeric expression specifying the number of class descriptors */
  printf("          bReportType     %2d\n", desc_hid->bReportType);     /**< Type of HID class report. */
  printf("          wReportLength   %2d\n", desc_hid->wReportLength);   /**< the total size of the Report descriptor. */

  // Endpoint descriptor
  p_desc = tu_desc_next(p_desc);
  auto desc_ep = (tusb_desc_endpoint_t const *) p_desc;
  for(int i = 0; i < desc_itf->bNumEndpoints; i++) {
    if (TUSB_DESC_ENDPOINT != desc_ep->bDescriptorType) {
      printf("        [ERROR] Bad endpoint type\n");
      return;
    }

    printf("      HID Endpoint Descriptor:\n");
    printf("        bLength         %8d\n", desc_ep->bLength);
    printf("        bDescriptorType %8d\n", desc_ep->bDescriptorType);
    printf("        bEndpointAddress    0x%02x %s\n", desc_ep->bEndpointAddress, bEndpointAddress_to_string(desc_ep->bEndpointAddress) );
    parse_bm_attributes( desc_ep, "      " );
    printf("        wMaxPacketSize    0x%04x\n", desc_ep->wMaxPacketSize );
    printf("        bInterval       %8d\n", desc_ep->bInterval );

    if(tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN) {
      // skip if failed to open endpoint
      if ( ! tuh_edpt_open(daddr, desc_ep) ) {
        printf("        [ERROR] Failed to open endpoint\n");
        return;
      }
      uint8_t* buf = get_hid_buf(daddr);
      if (!buf) {
        printf("        [ERROR] OOM\n");
        return; // out of memory
      }

      #pragma GCC diagnostic push
      #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
      tuh_xfer_t xfer =
      {
        .daddr       = daddr,
        .ep_addr     = desc_ep->bEndpointAddress,
        .buflen      = 64,
        .buffer      = buf,
        .complete_cb = hid_report_received,
        .user_data   = (uintptr_t) buf, // since buffer is not available in callback, use user data to store the buffer
      };
      #pragma GCC diagnostic pop
      // submit transfer for this EP
      tuh_edpt_xfer(&xfer);
      printf("        Listen to [dev %u: ep %02x]\r\n", daddr, desc_ep->bEndpointAddress);

    }
    p_desc = tu_desc_next(p_desc);
    desc_ep = (tusb_desc_endpoint_t const *) p_desc;
  }
}

