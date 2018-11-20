/*
 * form.cc
 *
 * Handling Forms
 *
 * by Simon Chenard
 * 2014.07.25
 */

#include <iostream>
#include <sstream>
#include <string>
#include "util/math.h"
#include "util/encoding.h"

#include "HTMLRenderer.h"
#include "util/namespace.h"
#include "util/misc.h"

namespace pdf2htmlEX {
   
using std::ofstream;
using std::cerr;
using std::min;
using std::max;

void HTMLRenderer::process_form(ofstream & out)
{
    FormPageWidgets * widgets = cur_catalog->getPage(pageNum)->getFormWidgets();
    int num = widgets->getNumWidgets();

    
    for(int i = 0; i < num; i++)
    {
        FormWidget * w = widgets->getWidget(i);
        
        
        if(w->getType() == formText)
        {
            double x1, y1, x2, y2;
            
            w->getRect(&x1, &y1, &x2, &y2);
            
            double width = x2 - x1;
            double height = y2 - y1;
            double font_size = height / 2;

            out << "<input id=\"text-" << pageNum << "-" << i 
                << "\" class=\"" << CSS::INPUT_TEXT_CN 
                << "\" type=\"text\" value=\"\""
                << " style=\"position: absolute; left: " << x1 
                << "px; bottom: " << y1 << "px;" 
                << " width: " << width << "px; height: " << std::to_string(height) 
                << "px; line-height: " << std::to_string(height) << "px; font-size: " 
                << font_size << "px;\" />" << endl;
        } 
        else if(w->getType() == formButton)
        {
            //Ideally would check w->getButtonType()
            //for more specific rendering
            
            AnnotWidget * annot = w->getWidgetAnnotation();
            LinkAction * link;
            
            if (annot->getAction()) {
                link = annot->getAction();
            } else {
                link = annot->getAdditionalAction(Annot::AdditionalActionsType::actionMousePressed);
            }
            if (link) {
                string dest_detail_str;
                string dest_str = get_linkaction_str(link, dest_detail_str);
            
                double x1, y1, x2, y2;
                w->getRect(&x1, &y1, &x2, &y2);
                
                double x, y, w, h, posX, posY, pageH;
                
                x = min<double>(x1, x2);
                y = min<double>(y1, y2);
                w = max<double>(x1, x2) - x;
                h = max<double>(y1, y2) - y;
                posX = x;
                posY = y2;
                pageH = html_text_page.get_height();

                tm_transform(default_ctm, posX, posY);

                posY = pageH - posY;
                
                out << "<a href=\"";
                writeAttribute(out, dest_str);
                out << "\"";
                
                if(!dest_detail_str.empty())
                    out << " data-dest-detail='" << dest_detail_str << "'";
                
                out << " class=\"" << CSS::LINK_CN << ' ' << CSS::CSS_DRAW_CN << ' ' << CSS::TRANSFORM_MATRIX_CN
                    << all_manager.transform_matrix.install(default_ctm)
                    << " " << CSS::LEFT_CN      << all_manager.left.install(round(posX))
                    << " " << CSS::TOP_CN    << all_manager.top.install(round(posY))
                    << " " << CSS::WIDTH_CN     << all_manager.width.install(round(w))
                    << " " << CSS::HEIGHT_CN    << all_manager.height.install(round(h))
                << "\" style=\"border-style:none;background:none;\"";
                
                
                out << "\"></a>";
            }

        }
        else 
        {
            cerr << "Unsupported form field detected" << endl;
        }
    }
}

}
