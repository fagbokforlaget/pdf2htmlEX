/*
 * image.cc
 *
 * Handling images
 *
 * by WangLu
 * 2012.08.14
 */

#include "HTMLRenderer.h"
#include "util/namespace.h"

namespace pdf2htmlEX {

void HTMLRenderer::drawImage(GfxState * state, Object * ref, Stream * str, int width, int height, GfxImageColorMap * colorMap, GBool interpolate, int *maskColors, GBool inlineImg)
{
    tracer.draw_image(state);

    return OutputDev::drawImage(state,ref,str,width,height,colorMap,interpolate,maskColors,inlineImg);

#if 1
    if(maskColors)
        return;
    const char * filename = str_fmt("%s/f%llx%s", param.dest_dir.c_str(), image_count++, ".jpg");
    FILE * f = fopen(filename, "wb");
    if(!f)
        throw string("Cannot open file for background image " ) + filename;

    std::unique_ptr<ImgWriter> writer;
    writer = std::unique_ptr<ImgWriter>(new JpegWriter);
    if(!writer->init(f, width, height, param.h_dpi, param.v_dpi))
        throw "Cannot initialize image writer";

    ImageStream * img_stream = new ImageStream(str, width, colorMap->getNumPixelComps(), colorMap->getBits());
    img_stream->reset();
    for(int i = 0; i < height; ++i)
    {
        auto p = img_stream->getLine();
        writer->writeRow(&p);
    }

    if(!writer->close())
    {
        throw "Cannot finish background image";
    }

    fclose(f);
#endif
}

void HTMLRenderer::drawSoftMaskedImage(GfxState *state, Object *ref, Stream *str,
                   int width, int height,
                   GfxImageColorMap *colorMap,
                   GBool interpolate,
                   Stream *maskStr,
                   int maskWidth, int maskHeight,
                   GfxImageColorMap *maskColorMap,
                   GBool maskInterpolate)
{
    tracer.draw_image(state);

    return OutputDev::drawSoftMaskedImage(state,ref,str, // TODO really required?
            width,height,colorMap,interpolate,
            maskStr, maskWidth, maskHeight, maskColorMap, maskInterpolate);
}

} // namespace pdf2htmlEX
