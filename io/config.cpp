#include "config.h"

#include <QFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>

Config config;

Config::Config()
    : saved_layout(false),
      show_track_lines(false),
      scroll_zooms(false),
      img_seq_formats("jpg|jpeg|bmp|tiff|tif|psd|png|tga|jp2|gif"),
      edit_tool_selects_links(false),
      edit_tool_also_seeks(false),
      select_also_seeks(false),
	  paste_seeks(true),
	  rectified_waveforms(false),
      default_transition_length(30),
      timecode_view(TIMECODE_DROP),
      show_title_safe_area(false),
      use_custom_title_safe_ratio(false),
	  custom_title_safe_ratio(1),
      enable_drag_files_to_timeline(false),
      autoscale_by_default(false)
{

}

void Config::load(QString path) {
    QFile f(path);
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        QXmlStreamReader stream(&f);

        while (!stream.atEnd()) {
            stream.readNext();
            if (stream.isStartElement()) {
                if (stream.name() == "SavedLayout") {
                    stream.readNext();
                    saved_layout = (stream.text() == "1");
                } else if (stream.name() == "ShowTrackLines") {
                    stream.readNext();
                    show_track_lines = (stream.text() == "1");
                } else if (stream.name() == "ScrollZooms") {
                    stream.readNext();
                    scroll_zooms = (stream.text() == "1");
                } else if (stream.name() == "EditToolSelectsLinks") {
                    stream.readNext();
                    edit_tool_selects_links = (stream.text() == "1");
                } else if (stream.name() == "EditToolAlsoSeeks") {
                    stream.readNext();
                    edit_tool_also_seeks = (stream.text() == "1");
                } else if (stream.name() == "SelectAlsoSeeks") {
                    stream.readNext();
                    select_also_seeks = (stream.text() == "1");
                } else if (stream.name() == "PasteSeeks") {
                    stream.readNext();
                    paste_seeks = (stream.text() == "1");
                } else if (stream.name() == "ImageSequenceFormats") {
                    stream.readNext();
                    img_seq_formats = stream.text().toString();
				} else if (stream.name() == "RectifiedWaveforms") {
                    stream.readNext();
					rectified_waveforms = (stream.text() == "1");
				} else if (stream.name() == "DefaultTransitionLength") {
                    stream.readNext();
                    default_transition_length = stream.text().toInt();
                } else if (stream.name() == "TimecodeView") {
                    stream.readNext();
                    timecode_view = stream.text().toInt();
                }else if (stream.name() == "ShowTitleSafeArea") {
                    stream.readNext();
                    show_title_safe_area = (stream.text() == "1");
                } else if (stream.name() == "UseCustomTitleSafeRatio") {
                    stream.readNext();
                    use_custom_title_safe_ratio = (stream.text() == "1");
                } else if (stream.name() == "CustomTitleSafeRatio") {
                    stream.readNext();
                    custom_title_safe_ratio = stream.text().toDouble();
				} else if (stream.name() == "EnableDragFilesToTimeline") {
					stream.readNext();
					enable_drag_files_to_timeline = (stream.text() == "1");;
                } else if (stream.name() == "AutoscaleByDefault") {
                    stream.readNext();
                    autoscale_by_default = (stream.text() == "1");;
                }
            }
        }
        if (stream.hasError()) {
            qDebug() << "[ERROR] Error parsing config XML." << stream.errorString();
        }

        f.close();
    }
	/*if (!custom_scale) {
		#ifdef _WIN32
		// Get Windows UI scale - TODO may not be compatible with XP
		HDC screen = GetDC(0);
		int dpiX = GetDeviceCaps (screen, LOGPIXELSX);
		//int dpiY = GetDeviceCaps (screen, LOGPIXELSY);

		scale = (float) dpiX / 96;

		ReleaseDC (0, screen);
		#endif
	}*/

//	padding *= scale;

	/*DIR* dir = opendir("conf");
	if (dir) {
		closedir(dir);
	} else if (ENOENT == errno) {
		// no config directory, assume this is the first time opening Olive
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "INFO", "NOTE: This version of Olive is INCOMPLETE and very likely to crash. It comes with NO WARRANTY or guarantee of functionality.\n\nRegardless, thanks for checking out the project and I hope you enjoy using it!", NULL);
	}*/
}

void Config::save(QString path) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        qDebug() << "[ERROR] Could not save configuration";
        return;
    }

    QXmlStreamWriter stream(&f);
    stream.setAutoFormatting(true);
    stream.writeStartDocument(); // doc
    stream.writeStartElement("Configuration"); // configuration

    stream.writeTextElement("Version", SAVE_VERSION);
    stream.writeTextElement("SavedLayout", QString::number(saved_layout));
    stream.writeTextElement("ShowTrackLines", QString::number(show_track_lines));
    stream.writeTextElement("ScrollZooms", QString::number(scroll_zooms));
    stream.writeTextElement("EditToolSelectsLinks", QString::number(edit_tool_selects_links));
    stream.writeTextElement("EditToolAlsoSeeks", QString::number(edit_tool_also_seeks));
    stream.writeTextElement("SelectAlsoSeeks", QString::number(select_also_seeks));
    stream.writeTextElement("PasteSeeks", QString::number(paste_seeks));
    stream.writeTextElement("ImageSequenceFormats", img_seq_formats);
	stream.writeTextElement("RectifiedWaveforms", QString::number(rectified_waveforms));
	stream.writeTextElement("DefaultTransitionLength", QString::number(default_transition_length));
    stream.writeTextElement("TimecodeView", QString::number(timecode_view));
    stream.writeTextElement("ShowTitleSafeArea", QString::number(show_title_safe_area));
    stream.writeTextElement("UseCustomTitleSafeRatio", QString::number(use_custom_title_safe_ratio));
    stream.writeTextElement("CustomTitleSafeRatio", QString::number(custom_title_safe_ratio));
	stream.writeTextElement("EnableDragFilesToTimeline", QString::number(enable_drag_files_to_timeline));
    stream.writeTextElement("AutoscaleByDefault", QString::number(autoscale_by_default));

    stream.writeEndElement();
    stream.writeEndDocument(); // doc
    f.close();
	/*#ifdef _WIN32
		_mkdir("conf");
	#else
		mkdir("conf", 0777);
	#endif*/

}
