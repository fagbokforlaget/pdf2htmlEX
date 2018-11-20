/*
 * CairoImagesRenderer.cc
 *
 * Copyright (C) 2012,2013 Lu Wang <coolwanglu@gmail.com>
 */

#include <string>
#include <fstream>
#include "goo/GooString.h"
#include "goo/PNGWriter.h"
#include "util/const.h"
#include "util/namespace.h"
#include "Error.h"
#ifdef ENABLE_LIBPNG
#include <png.h>
#endif

#include "pdf2htmlEX-config.h"

#include "Base64Stream.h"

#if ENABLE_SVG

#include "CairoImagesRenderer.h"

namespace pdf2htmlEX {

using std::string;
using std::ifstream;
using std::ofstream;
using std::vector;
using std::unordered_map;

CairoImagesRenderer::CairoImagesRenderer(HTMLRenderer * html_renderer, const Param & param)
    : CairoImageOutputDev()
    , html_renderer(html_renderer)
    , param(param)
    , surface(nullptr)
{ }

CairoImagesRenderer::~CairoImagesRenderer()
{
    for(auto const& p : bitmaps_ref_count)
    {
        if (p.second == 0)
        {
            html_renderer->tmp_files.add(this->build_bitmap_path(p.first));
        }
    }
}

void CairoImagesRenderer::drawChar(GfxState *state, double x, double y,
        double dx, double dy,
        double originX, double originY,
        CharCode code, int nBytes, Unicode *u, int uLen)
{
    // draw characters as image when
    // - in fallback mode
    // - OR there is special filling method
    // - OR using a writing mode font
    // - OR using a Type 3 font while param.process_type3 is not enabled
    // - OR the text is used as path
    if((param.fallback || param.proof)
        || ( (state->getFont())
            && ( (state->getFont()->getWMode())
                 || ((state->getFont()->getType() == fontType3) && (!param.process_type3))
                 || (state->getRender() >= 4)
               )
          )
      )
    {
        CairoImageOutputDev::drawChar(state,x,y,dx,dy,originX,originY,code,nBytes,u,uLen);
    }
    // If a char is treated as image, it is not subject to cover test
    // (see HTMLRenderer::drawString), so don't increase drawn_char_count.
    // else if (param.correct_text_visibility) {
    //     if (html_renderer->is_char_covered(drawn_char_count))
    //         CairoImageOutputDev::drawChar(state,x,y,dx,dy,originX,originY,code,nBytes,u,uLen);
    //     drawn_char_count++;
    // }
}

void CairoImagesRenderer::beginTextObject(GfxState *state)
{
    if (param.proof == 2)
        proof_begin_text_object(state, this);
    CairoImageOutputDev::beginTextObject(state);
}

void CairoImagesRenderer::beginString(GfxState *state, GooString * str)
{
    if (param.proof == 2)
        proof_begin_string(state, this);
    CairoImageOutputDev::beginString(state, str);
}

void CairoImagesRenderer::endTextObject(GfxState *state)
{
    if (param.proof == 2)
        proof_end_text_object(state, this);
    CairoImageOutputDev::endTextObject(state);
}

void CairoImagesRenderer::updateRender(GfxState *state)
{
    if (param.proof == 2)
        proof_update_render(state, this);
    CairoImageOutputDev::updateRender(state);
}

void CairoImagesRenderer::init(PDFDoc * doc)
{
    startDoc(doc);
}

static GBool annot_cb(Annot *, void * pflag) {
    return (*((bool*)pflag)) ? gTrue : gFalse;
};

bool CairoImagesRenderer::render_page(PDFDoc * doc, int pageno)
{
    drawn_char_count = 0;
    double page_width;
    double page_height;
    if(param.use_cropbox)
    {
        page_width = doc->getPageCropWidth(pageno);
        page_height = doc->getPageCropHeight(pageno);
    }
    else
    {
        page_width = doc->getPageMediaWidth(pageno);
        page_height = doc->getPageMediaHeight(pageno);
    }

    if (doc->getPageRotate(pageno) == 90 || doc->getPageRotate(pageno) == 270)
        std::swap(page_height, page_width);

    bitmaps_in_current_page.clear();

    bool process_annotation = param.process_annotation;
    doc->displayPage(this, pageno, param.actual_dpi, param.actual_dpi,
            0, 
            (!(param.use_cropbox)),
            false, 
            false,
            nullptr, nullptr, &annot_cb, &process_annotation);

    return true;
}

void CairoImagesRenderer::embed_image(int pageno)
{
    for (int i = 0; i < CairoImageOutputDev::getNumImages (); i++) {
        CairoImage *image = CairoImageOutputDev::getImage (i);
        surface = image->getImage ();

        {
            auto fn = html_renderer->str_fmt("%s/bg%x-%x.png", param.dest_dir.c_str(), pageno, i);
            writePageImage((char*)fn);
        }

        double x1, x2;	// image x coordinates
        double y1, y2;	// image y coordinates
        double h_scale = html_renderer->text_zoom_factor() * DEFAULT_DPI / param.actual_dpi;
        double v_scale = html_renderer->text_zoom_factor() * DEFAULT_DPI / param.actual_dpi;

        image->getRect (&x1, &y1, &x2, &y2);

        auto & f_page = *(html_renderer->f_curpage);
        auto & all_manager = html_renderer->all_manager;

        f_page << "<img class=\"" << CSS::BACKGROUND_IMAGE_CN 
             << " " << CSS::LEFT_CN      << all_manager.left.install(x1 * h_scale)
             << " " << CSS::TOP_CN    << all_manager.top.install(y1 * v_scale)
             << " " << CSS::WIDTH_CN     << all_manager.width.install((x2 - x1 - 1) * h_scale)
             << " " << CSS::HEIGHT_CN    << all_manager.height.install((y2 - y1 - 1) * v_scale)
            << "\" alt=\"\" src=\"";
        {
            f_page << (char*)html_renderer->str_fmt("bg%x-%x.png", pageno, i);
        }
        f_page << "\"/>";

        surface = nullptr;
    }
    CairoImageOutputDev::clearImages();
}

string CairoImagesRenderer::build_bitmap_path(int id)
{
    // "o" for "PDF Object"
    return string(html_renderer->str_fmt("%s/o%d.jpg", param.dest_dir.c_str(), id));
}
// Override CairoOutputDev::setMimeData() and dump bitmaps in SVG to external files.
void CairoImagesRenderer::setMimeData(GfxState *state, Stream *str, Object *ref,
				 GfxImageColorMap *colorMap, cairo_surface_t *image, int height)
{
    if (param.svg_embed_bitmap)
    {
        CairoImageOutputDev::setMimeData(state, str, ref, colorMap, image, cairo_image_surface_get_height (image));
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
    Object obj = str->getDict()->lookup("ColorSpace");
    if (!obj.isName() || (strcmp(obj.getName(), "DeviceRGB") && strcmp(obj.getName(), "DeviceGray")) )
    {
        //obj.free();
        return;
    }
    //obj.free();
    obj = str->getDict()->lookup("Decode");
    if (obj.isArray())
    {
        //obj.free();
        return;
    }
    //obj.free();

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

void CairoImagesRenderer::writePageImage(char * filename)
{
  ImgWriter *writer = 0;
  FILE *file;
  int height, width, stride;
  unsigned char *data;
  
  writer = new PNGWriter(PNGWriter::RGBA);
  static_cast<PNGWriter*>(writer)->setSRGBProfile();

  if (!writer)
    return;

  file = fopen(filename, "wb");

  if (!file) {
    fprintf(stderr, "Error opening output file \n");
    exit(2);
  }

  height = cairo_image_surface_get_height(surface);
  width = cairo_image_surface_get_width(surface);
  stride = cairo_image_surface_get_stride(surface);
  cairo_surface_flush(surface);
  data = cairo_image_surface_get_data(surface);

  if (!writer->init(file, width, height, 72, 72)) {
    fprintf(stderr, "Error writing \n");
    exit(2);
  }

  unsigned char *row = (unsigned char *) gmallocn(width, 4);

  for (int y = 0; y < height; y++ ) {
    uint32_t *pixel = (uint32_t *) (data + y*stride);
    unsigned char *rowp = row;
    int bit = 7;
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


} // namespace pdf2htmlEX

#endif // ENABLE_SVG

