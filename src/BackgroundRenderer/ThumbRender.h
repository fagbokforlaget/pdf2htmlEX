/*
 * Splash Background renderer
 * Render all those things not supported as Image, with Splash
 *
 * Copyright (C) 2012,2013 Lu Wang <coolwanglu@gmail.com>
 */


#ifndef THUMB_RENDER_H__
#define THUMB_RENDER_H__

#include <string>

#include <splash/SplashBitmap.h>
#include <SplashOutputDev.h>

#include "pdf2htmlEX-config.h"

#include "Param.h"
#include "HTMLRenderer/HTMLRenderer.h"

namespace pdf2htmlEX {

// Based on BackgroundRenderer from poppler
class ThumbRenderer : public BackgroundRenderer, SplashOutputDev 
{
public:
  static const SplashColor white;
  //format: "png" or "jpg", or "" for a default format
  ThumbRenderer(const std::string & format, HTMLRenderer * html_renderer, const Param & param);

  virtual ~ThumbRenderer() { }

  virtual void init(PDFDoc * doc);
  virtual bool render_page(PDFDoc * doc, int pageno);
  virtual void embed_image(int pageno);
  virtual void startPage(int pageNum, GfxState *state, XRef *xrefA);


protected:
  void dump_image(const char * filename, int x1, int y1, int x2, int y2);
  HTMLRenderer * html_renderer;
  const Param & param;
  std::string format;
};

} // namespace pdf2htmlEX

#endif // THUMB_RENDER_H__
