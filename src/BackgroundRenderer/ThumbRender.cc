/*
 * ThumbRender.cc
 *
 * Copyright (C) 2012,2013 Lu Wang <coolwanglu@gmail.com>
 */

#include <fstream>
#include <vector>
#include <memory>

#include <poppler-config.h>
#include <PDFDoc.h>
#include <goo/PNGWriter.h>
#include <goo/JpegWriter.h>

#include "Base64Stream.h"
#include "util/const.h"

#include "ThumbRender.h"

namespace pdf2htmlEX {

using std::string;
using std::ifstream;
using std::vector;
using std::unique_ptr;

const SplashColor ThumbRenderer::white = {255,255,255};

ThumbRenderer::ThumbRenderer(const string & imgFormat, HTMLRenderer * html_renderer, const Param & param)
    : SplashOutputDev(splashModeRGB8, 4, gFalse, (SplashColorPtr)(&white))
    , html_renderer(html_renderer)
    , param(param)
    , format(imgFormat)
{
    
}

/*
 * SplashOutputDev::startPage would paint the whole page with the background color
 * And thus have modified region set to the whole page area
 * We do not want that.
 */
void ThumbRenderer::startPage(int pageNum, GfxState *state, XRef *xrefA)
{
    SplashOutputDev::startPage(pageNum, state, xrefA);
    clearModRegion();
}

void ThumbRenderer::init(PDFDoc * doc)
{
    startDoc(doc);
}

static GBool annot_cb(Annot *, void * pflag) {
    return (*((bool*)pflag)) ? gTrue : gFalse;
};

bool ThumbRenderer::render_page(PDFDoc * doc, int pageno)
{
    double page_width;
    double page_height;
    double h_res;
    double v_res;
    
    page_width = html_renderer->html_text_page.get_width() * param.h_dpi / DEFAULT_DPI;
    page_height = html_renderer->html_text_page.get_height() * param.v_dpi / DEFAULT_DPI;
    h_res = (html_renderer->text_zoom_factor() * param.h_dpi * 200) / page_width;
    v_res = (html_renderer->text_zoom_factor() * param.v_dpi * 150) / page_height;
    
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
    // xmin->xmax is top->bottom
    int xmin, xmax, ymin, ymax;
    getModRegion(&xmin, &ymin, &xmax, &ymax);

    // dump the background image only when it is not empty
    if((xmin <= xmax) && (ymin <= ymax))
    {
        {
            auto fn = html_renderer->str_fmt("%s/thumbs/slide%d.png", param.dest_dir.c_str(), pageno);
            

            dump_image((char*)fn, xmin, ymin, xmax, ymax);
        }
    }
}

// There might be mem leak when exception is thrown !
void ThumbRenderer::dump_image(const char * filename, int x1, int y1, int x2, int y2)
{
    int width = x2 - x1 + 1;
    int height = y2 - y1 + 1;
    if((width <= 0) || (height <= 0))
        throw "Bad metric for background image";

    FILE * f = fopen(filename, "wb");
    if(!f)
        throw string("Cannot open file for background image " ) + filename;

    // use unique_ptr to auto delete the object upon exception
    unique_ptr<ImgWriter> writer;

    
    writer = unique_ptr<ImgWriter>(new PNGWriter);
   

    if(!writer->init(f, width, height, param.h_dpi, param.v_dpi))
        throw "Cannot initialize image writer";
        
    auto * bitmap = getBitmap();
    assert(bitmap->getMode() == splashModeRGB8);

    SplashColorPtr data = bitmap->getDataPtr();
    int row_size = bitmap->getRowSize();

    vector<unsigned char*> pointers;
    pointers.reserve(height);
    SplashColorPtr p = data + y1 * row_size + x1 * 3;
    for(int i = 0; i < height; ++i)
    {
        pointers.push_back(p);
        p += row_size;
    }
    
    if(!writer->writePointers(pointers.data(), height)) 
    {
        throw "Cannot write background image";
    }

    if(!writer->close())
    {
        throw "Cannot finish background image";
    }

    fclose(f);
}

} // namespace pdf2htmlEX
