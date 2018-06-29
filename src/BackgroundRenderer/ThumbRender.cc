/*
 * ThumbRender.cc
 *
 * Copyright (C) 2012,2013 Lu Wang <coolwanglu@gmail.com>
 */

#include <string>
#include <fstream>
#include "goo/PNGWriter.h"

#include "pdf2htmlEX-config.h"

#include "Base64Stream.h"
#ifdef ENABLE_LIBPNG
#include <png.h>
#endif


#include "ThumbRender.h"
#include "SplashBackgroundRenderer.h"

namespace pdf2htmlEX {

using std::string;
using std::ifstream;
using std::ofstream;
using std::vector;
using std::unordered_map;

ThumbRenderer::ThumbRenderer(HTMLRenderer * html_renderer, const Param & param)
    : CairoOutputDev()
    , html_renderer(html_renderer)
    , param(param)
    , surface(nullptr)
{ }

ThumbRenderer::~ThumbRenderer()
{
    for(auto const& p : bitmaps_ref_count)
    {
        if (p.second == 0)
        {
            html_renderer->tmp_files.add(this->build_bitmap_path(p.first));
        }
    }
}

void ThumbRenderer::init(PDFDoc * doc)
{
    startDoc(doc);
}

static GBool annot_cb(Annot *, void * pflag) {
    return (*((bool*)pflag)) ? gFalse : gTrue;
};

bool ThumbRenderer::render_page(PDFDoc * doc, int pageno)
{
    double page_width;
    double page_height;
    double h_res;
    double v_res;
    
    page_width = html_renderer->html_text_page.get_width() * param.h_dpi / DEFAULT_DPI;
    page_height = html_renderer->html_text_page.get_height() * param.v_dpi / DEFAULT_DPI;
    h_res = (html_renderer->text_zoom_factor() * param.h_dpi * 400) / param.fit_width;
    v_res = (html_renderer->text_zoom_factor() * param.v_dpi * 300) / param.fit_height;

    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 400, 300);
    cairo_t * cr = cairo_create(surface);
    setCairo(cr);
    
    bool process_annotation = param.process_annotation;
    doc->displayPage(this, pageno, h_res, v_res,
            0, 
            (!(param.use_cropbox)),
            false, false,
            nullptr, nullptr, &annot_cb, &process_annotation);
    return true;
}

void ThumbRenderer::embed_image(int pageno)
{
  auto fn = html_renderer->str_fmt("%s/thumbs/slide%d.png", param.dest_dir.c_str(), pageno);

  FILE *file;
  int height, width, stride;
  unsigned char *data;
  ImgWriter *writer = 0;
  
  writer = new PNGWriter(PNGWriter::RGBA);
  static_cast<PNGWriter*>(writer)->setSRGBProfile();

  if (!writer)
    return;

  file = fopen((char*)fn, "wb");

  if (!file) {
    fprintf(stderr, "Error opening output file \n");
    exit(2);
  }

  height = cairo_image_surface_get_height(surface);
  width = cairo_image_surface_get_width(surface);
  stride = cairo_image_surface_get_stride(surface);
  cairo_surface_flush(surface);
  data = cairo_image_surface_get_data(surface);

  if (!writer->init(file, width, height, 144, 144)) {
    fprintf(stderr, "Error writing \n");
    exit(2);
  }

  unsigned char *row = (unsigned char *) gmallocn(width, 4);

  for (int y = 0; y < height; y++ ) {
    uint32_t *pixel = (uint32_t *) (data + y*stride);
    unsigned char *rowp = row;
    for (int x = 0; x < width; x++, pixel++) {
	// unpremultiply into RGBA format
          uint8_t a;
          a = (*pixel & 0xff000000) >> 24;
          if (a == 0) {
            *rowp++ = 0;
            *rowp++ = 0;
            *rowp++ = 0;
          } else {
            *rowp++ = (((*pixel & 0xff0000) >> 16) * 255 + a / 2) / a;
            *rowp++ = (((*pixel & 0x00ff00) >>  8) * 255 + a / 2) / a;
            *rowp++ = (((*pixel & 0x0000ff) >>  0) * 255 + a / 2) / a;
          }
          *rowp++ = a;
    }
    writer->writeRow(&row);
  }
  gfree(row);
  writer->close();
  delete writer;
  if (file == stdout) fflush(file);
  else fclose(file);
}

string ThumbRenderer::build_bitmap_path(int id)
{
    // "o" for "PDF Object"
    return string(html_renderer->str_fmt("%s/o%d.jpg", param.dest_dir.c_str(), id));
}
// Override CairoOutputDev::setMimeData() and dump bitmaps in SVG to external files.
void ThumbRenderer::setMimeData(Stream *str, Object *ref, cairo_surface_t *image)
{
    if (param.svg_embed_bitmap)
    {
       CairoOutputDev::setMimeData(str, ref, image);
        return;
    }

    // TODO dump bitmaps in other formats.
    if (str->getKind() != strDCT)
        return;

    // TODO inline image?
    if (ref == nullptr || !ref->isRef())
        return;

    // We only dump rgb or gray jpeg without /Decode array.
    //
    // Although jpeg support CMYK, PDF readers do color conversion incompatibly with most other
    // programs (including browsers): other programs invert CMYK color if 'Adobe' marker (app14) presents
    // in a jpeg file; while PDF readers don't, they solely rely on /Decode array to invert color.
    // It's a bit complicated to decide whether a CMYK jpeg is safe to dump, so we don't dump at all.
    // See also:
    //   JPEG file embedded in PDF (CMYK) https://forums.adobe.com/thread/975777
    //   http://stackoverflow.com/questions/3123574/how-to-convert-from-cmyk-to-rgb-in-java-correctly
    //
    // In PDF, jpeg stream objects can also specify other color spaces like DeviceN and Separation,
    // It is also not safe to dump them directly.
    Object obj;
    str->getDict()->lookup("ColorSpace", &obj);
    if (!obj.isName() || (strcmp(obj.getName(), "DeviceRGB") && strcmp(obj.getName(), "DeviceGray")) )
    {
        obj.free();
        return;
    }
    obj.free();
    str->getDict()->lookup("Decode", &obj);
    if (obj.isArray())
    {
        obj.free();
        return;
    }
    obj.free();

    int imgId = ref->getRef().num;
    auto uri = strdup((char*) html_renderer->str_fmt("o%d.jpg", imgId));
    auto st = cairo_surface_set_mime_data(image, CAIRO_MIME_TYPE_URI,
        (unsigned char*) uri, strlen(uri), free, uri);
    if (st)
    {
        free(uri);
        return;
    }
    bitmaps_in_current_page.push_back(imgId);

    if(bitmaps_ref_count.find(imgId) != bitmaps_ref_count.end())
        return;

    bitmaps_ref_count[imgId] = 0;

    char *strBuffer;
    int len;
    if (getStreamData(str->getNextStream(), &strBuffer, &len))
    {
        ofstream imgfile(build_bitmap_path(imgId), ofstream::binary);
        imgfile.write(strBuffer, len);
        free(strBuffer);
    }
}

} // namespace pdf2htmlEX



