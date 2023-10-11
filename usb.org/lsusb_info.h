#pragma once

#include "./lsusb.ids.h"
#include "./lsusb.classes_protos.h"

const size_t usb_vids_count       = sizeof(usb_vids)/sizeof(vendor_id_t);
const size_t usb_pids_count       = sizeof(usb_pids)/sizeof(product_id_t);
const size_t usb_classes_count    = sizeof(usb_classes)/sizeof(usb_class_t);
const size_t usb_subclasses_count = sizeof(usb_subclasses)/sizeof(usb_subclass_t);
const size_t usb_protos_count     = sizeof(usb_protos)/sizeof(usb_proto_t);

const char* bmAttrXfer[4]  = {"Control", "Isochronous", "Bulk", "Interrupt"};
const char* bmAttrSync[4]  = {"None", "Asynchronous", "Adaptive", "Synchronous"};
const char* bmAttrUsage[4] = {"Data", "Feedback", "Explicit feedback", "Reserved"};

const vendor_id_t      nullVendor = { 0, "", 0, 0 };
const usb_class_t       nullClass = { 0, "", 0, 0 };
const usb_subclass_t nullSubclass = { 0, "", 0, 0 };
const product_id_t    nullProduct = { 0, "" };
const usb_proto_t       nullProto = { 0, "" };

struct usb_vid_pid_t
{
  const vendor_id_t *vendor;
  const product_id_t *product;
};

struct usb_class_sub_proto_t
{
  const usb_class_t* dev_class;
  const usb_subclass_t* dev_subclass;
  const usb_proto_t* dev_proto;
};

struct bEndpointAddress_t
{
  struct bits_t {
    uint8_t ep_num : 4;
    uint8_t res    : 3;
    uint8_t dir    : 1;
  };
  union {
    uint8_t address;
    bits_t bits;
  };
};



const usb_class_t* get_class( uint8_t class_id )
{
  for( size_t i=0; i<usb_classes_count; i++ ) {
    if( usb_classes[i].class_id == class_id ) return &usb_classes[i];
  }
  return &nullClass;
}


const usb_subclass_t* get_subclass( uint8_t subclass_id, uint8_t offset, uint8_t count )
{
  if( count>0 && offset+count<=usb_subclasses_count ) {
    for( int i=offset; i<offset+count; i++ ) {
      if( usb_subclasses[i].subclass_id == subclass_id ) return &usb_subclasses[i];
    }
  }
  return &nullSubclass;
}


const usb_proto_t* get_proto( uint8_t proto_id, uint8_t offset, uint8_t count )
{
  if( count>0 && offset+count<=usb_protos_count ) {
    for( int i=offset; i<offset+count; i++ ) {
      if( usb_protos[i].proto_id == proto_id ) return &usb_protos[i];
    }
  }
  return &nullProto;
}


usb_class_sub_proto_t get_class_sub_proto( uint8_t class_id, uint8_t subclass_id, uint8_t proto_id )
{
  const usb_class_t* dev_class = get_class( class_id );
  const usb_subclass_t* dev_subclass = &nullSubclass;
  const usb_proto_t* dev_proto = &nullProto;
  if( dev_class && dev_class->subclass_count > 0 ) {
    dev_subclass = get_subclass( subclass_id, dev_class->subclass_t_idx, dev_class->subclass_count );//&usb_subclasses[dev_class->subclass_t_idx];
    if( dev_subclass && dev_subclass->proto_count > 0 ) {
      dev_proto = get_proto( proto_id, dev_subclass->proto_t_idx, dev_subclass->proto_count );// &usb_protos[dev_subclass->proto_t_idx];
    }
  }
  return { dev_class, dev_subclass, dev_proto };
}


const vendor_id_t* get_vendor( uint16_t vendor_id )
{
  uint16_t maybe_idx = map( vendor_id, usb_vids[0].vendor_id, usb_vids[usb_vids_count-1].vendor_id, 0, usb_vids_count-1 );
  if( maybe_idx >= usb_vids_count ) return &nullVendor; // overflow
  if( usb_vids[maybe_idx].vendor_id == vendor_id ) return &usb_vids[maybe_idx]; // match!
  int dir      = vendor_id < usb_vids[maybe_idx].vendor_id ? -1 : 1; // seek direction
  int last_idx = vendor_id < usb_vids[maybe_idx].vendor_id ?  0 : usb_vids_count-1;
  do {
    maybe_idx += dir;
    if( usb_vids[maybe_idx].vendor_id == vendor_id ) return &usb_vids[maybe_idx]; // match!
  } while( maybe_idx!=last_idx );
  return &nullVendor;
}


const product_id_t* get_product( const vendor_id_t *vendor, uint16_t product_id )
{
  if( vendor->product_count==1 ) return &usb_pids[vendor->product_id_t_idx];

  for( size_t i=0; i<vendor->product_count; i++ ) {
    auto product = &usb_pids[vendor->product_id_t_idx+i];
    if( product->product_id == product_id ) return product;
  }
  return &nullProduct;
}


usb_vid_pid_t get_vid_pid( uint16_t vendor_id, uint16_t product_id )
{
  const vendor_id_t *vendor = get_vendor( vendor_id );
  const product_id_t *product = &nullProduct;

  if( vendor && vendor->product_count>0 ) {
    //product = &usb_pids[vendor->product_id_t_idx];
    product = get_product( vendor, product_id );
  }
  return {vendor, product};
}


const char* vendor_id_to_string( uint16_t vendor_id )
{
  uint16_t maybe_idx = map( vendor_id, usb_vids[0].vendor_id, usb_vids[usb_vids_count-1].vendor_id, 0, usb_vids_count-1 );
  if( maybe_idx >= usb_vids_count ) return nullptr; // overflow
  if( usb_vids[maybe_idx].vendor_id == vendor_id ) return usb_vids[maybe_idx].name; // match!
  int dir      = vendor_id < usb_vids[maybe_idx].vendor_id ? -1 : 1; // seek direction
  int last_idx = vendor_id < usb_vids[maybe_idx].vendor_id ?  0 : usb_vids_count-1;
  do {
    maybe_idx += dir;
    if( usb_vids[maybe_idx].vendor_id == vendor_id ) return usb_vids[maybe_idx].name; // match!
  } while( maybe_idx!=last_idx );
  return nullptr;
}


const char* bEndpointAddress_to_string( uint8_t address )
{
  static char ep_str[32];
  bEndpointAddress_t addr = {address};
  sprintf(ep_str, "EP %d %s", addr.bits.ep_num, addr.bits.dir==0?"OUT":"IN");
  return ep_str;
}


void parse_bm_attributes( tusb_desc_endpoint_t const * desc_ep, const char*spacing )
{
  int XferIdx  = desc_ep->bmAttributes.xfer  < 4 ? desc_ep->bmAttributes.xfer  : 0;
  int SyncIdx  = desc_ep->bmAttributes.sync  < 4 ? desc_ep->bmAttributes.sync  : 0;
  int UsageIdx = desc_ep->bmAttributes.usage < 4 ? desc_ep->bmAttributes.usage : 0;

  printf("%s  bmAttributes:\n", spacing );
  printf("%s    Transfer Type: %s\n", spacing, bmAttrXfer[XferIdx]   );
  printf("%s    Synch Type:    %s\n", spacing, bmAttrSync[SyncIdx]   );
  printf("%s    Usage Type:    %s\n", spacing, bmAttrUsage[UsageIdx] );
}


const char *bJackType_to_string( uint8_t type )
{
  return( type==MIDI_JACK_EMBEDDED) ? "Embedded" : "External";
}


const char *midi_desc_subtype_to_string( uint8_t type )
{
  switch(type) {
    case MIDI_CS_INTERFACE_HEADER  : return "HEADER";
    case MIDI_CS_INTERFACE_IN_JACK : return "MIDI_IN_JACK";
    case MIDI_CS_INTERFACE_OUT_JACK: return "MIDI_OUT_JACK";
    case MIDI_CS_INTERFACE_ELEMENT : return "MIDI_ELEMENT";
    default: break;
  }
  return "";
}
