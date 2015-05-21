#include <data.h>
#include <yaml-cpp/yaml.h>

#define CUR_VERSION "1.0"

namespace iris {
namespace data {

iris::data::store iris::data::store::default_store() {

    fs::file home = fs::file::home_directory();
    fs::file base = home.child(".config/iris");
    fs::file fver = base.child("/version");

    if (!base.exists() || !fver.exists()) {
        throw std::runtime_error("Could not initialize store");
    }

    std::string sver = fver.read_all();
    sver.erase(std::find_if(sver.begin(), sver.end(), [](const char c) {
        return c == ' ' || c == '\n' || c == '\t';
    }));

    if (sver != CUR_VERSION) {
        std::cerr << "[W] different store version: " ;
        std::cerr << sver << " [" << CUR_VERSION << "]" << std::endl;
    }

    return data::store(base);
}


iris::data::store::store(const fs::file &path) : base(path) {

}

std::string iris::data::store::default_monitor() const {

    fs::file dm_link = base.child("default.monitor");

    if (!dm_link.exists()) {
        throw std::runtime_error("cfg: no default monitor found @ " + dm_link.path());
    }

    fs::file target = dm_link.readlink();
    return target.name();
}


std::vector<std::string> store::list_monitors() const {
    fs::file mdir = base.child("monitors");

    std::vector<fs::file> res;
    std::copy_if(mdir.children().begin(), mdir.children().end(), std::back_inserter(res), [](const fs::file &f){
        fs::file mfile = f.child("/" + f.name() + ".monitor");
        return mfile.exists();
    });

    std::vector<std::string> names(res.size());
    std::transform(res.begin(), res.end(), names.begin(), [](const fs::file &f){
        return f.name();
    });

    return names;
}

iris::data::monitor iris::data::store::load_monitor(const std::string &uid) const {
    fs::file mfs = base.child("monitors/" + uid + "/" + uid + ".monitor");
    std::cout << mfs.path() << std::endl;
    return yaml2monitor(mfs.read_all());
}


std::string store::latest_settings(const monitor &monitor) const {
    std::vector<std::string> setting_ids = list_settings(monitor);
    if (setting_ids.empty()) {
        return std::string("");
    }

    return setting_ids.front();
}

std::vector<std::string> store::list_settings(const monitor &monitor) const {

    fs::file mdir = base.child(monitor.qualified_id());

    std::vector<fs::file> res;
    std::copy_if(mdir.children().begin(), mdir.children().end(),
                 std::back_inserter(res), fs::fn_matcher("*.settings"));

    std::sort(res.begin(), res.end(), [](const fs::file &a, const fs::file &b) {
        return a.name() > b.name();
    });

    std::vector<std::string> ids;
    std::transform(res.begin(), res.end(), std::back_inserter(ids), [](const fs::file &f) {
        std::string name = f.name();
        size_t len = name.size();
        name.erase(len - 9, 9); // we matched for "*.settings"
        return name;
    });

    return ids;
}

rgb2lms store::load_rgb2lms(const display &display) const {

    // need to find the latest calibration that fits the display

    fs::file mdir = base.child("monitors/" + display.monitor_id);
    
    std::vector<fs::file> res;
    std::copy_if(mdir.children().begin(), mdir.children().end(),
              std::back_inserter(res), fs::fn_matcher("*.rgb2lms"));

    std::sort(res.begin(), res.end(), [](const fs::file &a, const fs::file &b) {
        return a.name() > b.name();
    });

    for (const fs::file f : res) {
        std::string data = f.read_all();
        rgb2lms ca = yaml2rgb2lms(data);

        const struct display &d = ca.dsy;

        if (d.monitor_id  == display.monitor_id &&
            d.settings_id == display.settings_id &&
            d.link_id     == display.link_id &&
            d.gfx         == display.gfx) {

            //FIXME:: check mode too
            return ca;
        }
    }

    throw std::runtime_error("Could not find any matching rgb2lms matrix");
}

display store::make_display(const monitor       &monitor,
                            const monitor::mode &mode,
                            const std::string   &gfx) const
{

    fs::file linkfile = base.child("links.cfg");
    std::string data = linkfile.read_all();
    YAML::Node root = YAML::Load(data);

    YAML::Node gfx_node = root[gfx];
    YAML::Node link_node = gfx_node[monitor.identifier()];

    std::string link_id = link_node.as<std::string>();

    display dsp;
    dsp.link_id = link_id;
    dsp.monitor_id = monitor.identifier();
    dsp.settings_id = latest_settings(monitor);
    dsp.mode = mode;
    dsp.gfx = gfx;

    return dsp;
}

// yaml stuff

static iris::data::monitor::mode yaml2mode(const YAML::Node &node) {
    iris::data::monitor::mode mode;

    mode.width = node["width"].as<float>();
    mode.height = node["height"].as<float>();
    mode.refresh = node["refresh"].as<float>();

    YAML::Node depth = node["color-depth"];
    mode.r = depth[0].as<int>();
    mode.g = depth[1].as<int>();
    mode.b = depth[2].as<int>();

    return mode;
}

iris::data::monitor iris::data::store::yaml2monitor(const std::string &data) {
    YAML::Node root = YAML::Load(data);

    YAML::Node start = root["monitor"];
    monitor monitor(start["id"].as<std::string>());

    monitor.name = start["name"].as<std::string>();
    monitor.vendor = start["vendor"].as<std::string>();
    monitor.year = start["year"].as<std::string>();

    monitor.default_mode = yaml2mode(start["preferred_mode"]);

    return monitor;
}

static display yaml2display(const YAML::Node &root) {

    display display;
    display.gfx = root["gfx"].as<std::string>();
    display.link_id = root["link_id"].as<std::string>();
    display.monitor_id = root["monitor_id"].as<std::string>();
    display.settings_id = root["settings_id"].as<std::string>();
    display.mode = yaml2mode(root["mode"]);

    return display;

}

static std::pair<float, float> yaml2size(const YAML::Node &root) {
    float width = root["width"].as<float>();
    float height = root["height"].as<float>();
    return std::make_pair(width, height);
}

rgb2lms store::yaml2rgb2lms(const std::string &data) {
    YAML::Node doc = YAML::Load(data);
    YAML::Node root = doc["rgb2lms"];

    rgb2lms calibration(root["id"].as<std::string>());
    std::tie(calibration.width, calibration.height) = yaml2size(root["size"]);
    calibration.dsy = yaml2display(root["display"]);

    std::string csv = root["dkl"].as<std::string>();
    calibration.dkl_params = dkl::parameter::from_csv_data(csv);

    return calibration;
}

static void emit_mode(const monitor::mode &mode, YAML::Emitter &out) {
    out << YAML::BeginMap;
    out << "width" << mode.width;
    out << "height" << mode.height;
    out << "refresh" << mode.refresh;
    out << "color-depth";
    out << YAML::Flow;
    out << YAML::BeginSeq << mode.r << mode.g << mode.b << YAML::EndSeq;
    out << YAML::EndMap; //mode
}

std::string iris::data::store::monitor2yaml(const iris::data::monitor &monitor) {
    YAML::Emitter out;

    out << YAML::BeginMap;
    out << "monitor";

    out << YAML::BeginMap;
    out << "id" << monitor.identifier();
    out << "name" << monitor.name;
    out << "vendor" << monitor.vendor;
    out << "year" << monitor.year;
    out << "serial" << monitor.serial;

    out << "preferred_mode";
    emit_mode(monitor.default_mode, out);
    out << YAML::EndMap; //monitor

    return std::string(out.c_str());
}

static void emit_display(const display &display, YAML::Emitter &out) {

    out << YAML::BeginMap;
    out << "monitor_id" << display.monitor_id;
    out << "settings_id" << display.settings_id;
    out << "link_id" << display.link_id;

    out << "gfx" << display.gfx;
    out << "mode";
    emit_mode(display.mode, out);
    out << YAML::EndMap;
}

std::string store::display2yaml(const display &display) {
    YAML::Emitter out;

    out << YAML::BeginMap;
    out << "display";
    emit_display(display, out);
    out << YAML::EndMap; //doc

    return std::string(out.c_str());
}


std::string store::rgb2lms2yaml(const rgb2lms &rgb2lms) {
    YAML::Emitter out;

    out << YAML::BeginMap;
    out << "rgb2lms";
    out << YAML::BeginMap;
    out << "id" << rgb2lms.identifier();
    out << "size" << YAML::BeginMap;
    out << "width" << rgb2lms.width;
    out << "height" << rgb2lms.height;
    out << YAML::EndMap; // size
    out << "gray-level" << rgb2lms.gray_level;
    out << "dataset" << rgb2lms.dataset;

    out << "display";
    emit_display(rgb2lms.dsy, out);

    std::stringstream buffer;
    rgb2lms.dkl_params.print(buffer);

    out << "dkl" << YAML::Literal << buffer.str();

    out << YAML::EndMap;
    out << YAML::EndMap;

    return std::string(out.c_str());
}


} //iris::cfg::
} //iris::