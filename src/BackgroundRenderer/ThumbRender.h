/*
 * ThumbRender renderer
 * Render all those things not supported as Image, with Cairo
 *
 * Copyright (C) 2012,2013 Lu Wang <coolwanglu@gmail.com>
 */


#ifndef THUMB_RENDER_H__
#define THUMB_RENDER_H__

#include <CairoOutputDev.h>
#include <cairo.h>
#include <cairo-svg.h>
#include <unordered_map>
#include <vector>
#include <string>

#include "pdf2htmlEX-config.h"

#include "Param.h"
#include "HTMLRenderer/HTMLRenderer.h"

namespace pdf2htmlEX {

// Based on BackgroundRenderer from poppler
class ThumbRenderer : public BackgroundRenderer, CairoOutputDev 
{
public:
  ThumbRenderer(HTMLRenderer * html_renderer, const Param & param);

  virtual ~ThumbRenderer();

  virtual void init(PDFDoc * doc);
  virtual bool render_page(PDFDoc * doc, int pageno);
  virtual void embed_image(int pageno);
  virtual GBool interpretType3Chars() { return !param.process_type3; }

protected:
  virtual void setMimeData(GfxState *state, Stream *str, Object *ref,
				 GfxImageColorMap *colorMap, cairo_surface_t *image, int height);

protected:
  HTMLRenderer * html_renderer;
  const Param & param;
  cairo_surface_t * surface;

private:
  // convert bitmap stream id to bitmap file name. No pageno prefix,
  // because a bitmap may be shared by multiple pages.
  std::string build_bitmap_path(int id);
  // map<id_of_bitmap_stream, usage_count_in_all_svgs>
  // note: if a svg bg fallbacks to bitmap bg, its bitmaps are not taken into account.
  std::unordered_map<int, int> bitmaps_ref_count;
  // id of bitmaps' stream used by current page
  std::vector<int> bitmaps_in_current_page;
  int drawn_char_count;
};

}

#endif //THUMB_RENDER_H__
