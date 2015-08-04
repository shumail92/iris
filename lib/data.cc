#include <data.h>
#include <yaml-cpp/yaml.h>
#include <csv.h>
#include <misc.h>

#define CUR_VERSION "1.0"

namespace iris {
namespace data {

static fs::file find_default_location() {

    static std::vector<fs::file> known_files = {
            fs::file("/etc/iris"),
            fs::file("~/.config/iris"),
            fs::file("~/experiments/config"),
            fs::file("~/experiments/iris")
    };

    //std::cerr << "[D] Looking for iris file in: " << std::endl;
    for (fs::file &f : known_files) {
        fs::file fver = f.child("/version");
        if (f.exists() && fver.exists()) {
            return f;
        }
    }

    return fs::file();
}


iris::data::store iris::data::store::default_store() {

    fs::file base = find_default_location();
    fs::file fver = base.child("/version");

    if (!base.exists() || !fver.exists()) {
        throw std::runtime_error("Could not initialize store");
    }

    data::store store(base);
    std::string sver = store.version_string();

    if (sver != CUR_VERSION) {
        std::cerr << "[W] different store version: " ;
        std::cerr << sver << " [" << CUR_VERSION << "]" << std::endl;
    }

    return store;
}


iris::data::store::store(const fs::file &path) : base(path) {

}


std::string store::version_string() const {
    fs::file fver = base.child("/version");

    std::string sver = fver.read_all();
    sver.erase(std::find_if(sver.begin(), sver.end(), [](const char c) {
        return c == ' ' || c == '\n' || c == '\t';
    }));

    return sver;
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

    fs::file mdir = base.child("monitors/" + monitor.qualified_id());

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

    throw std::runtime_error("Could not find any matching rgb2lms matrix ["
                             + display.monitor_id + ", " + display.settings_id + ", "
                             + display.link_id + ", " + display.gfx + "]");
}


fs::file store::store_rgb2lms(const rgb2lms &rgb2lms) {
    display display = rgb2lms.dsy;

    monitor monitor;
    try {
        monitor = load_monitor(display.monitor_id);
    } catch(const std::runtime_error &e) {
        throw std::runtime_error("Could not import rgb2lms; monitor loading error");
    }

    fs::file mdir = base.child("monitors/" + display.monitor_id);
    fs::file fd = mdir.child(rgb2lms.identifier() + ".rgb2lms");

    if (fd.exists()) {
        throw std::runtime_error("rgb2lms data already exists!");
    }

    std::string data = rgb2lms2yaml(rgb2lms);
    fd.write_all(data);

    return fd;
}

subject store::load_subject(const std::string &uid) {
    fs::file sfile = base.child("subjects/" + uid + "/" + uid + ".subject");
    return yaml2subject(sfile.read_all());
}


std::vector<iris::data::subject> store::find_subjects(const std::string &phrase) {
    fs::file sdir = base.child("subjects");

    std::vector<fs::file> res;
    std::copy_if(sdir.children().begin(), sdir.children().end(),
                 std::back_inserter(res),
                 [](const fs::file &f) {
                     const std::string &n = f.name();
                     if (n == "." || n == ".." || ! f.is_directory()) {
                         return false;
                     }

                     fs::file sf = f.child(n + ".subject");
                     return sf.exists();
                 });

    std::vector<subject> subjects;
    std::transform(res.cbegin(), res.cend(), std::back_inserter(subjects),
                   [this](const fs::file &f) {
                       return load_subject(f.name());
                   });

    std::vector<subject> hits;
    std::copy_if(subjects.cbegin(), subjects.cend(), std::back_inserter(hits),
                 [&phrase](const subject &s) {
                     return s.name == phrase ||
                            s.initials == phrase ||
                            s.identifier() == phrase ||
                            s.qualified_id() == phrase;
                 });

    return hits;
}

isoslant store::load_isoslant(const subject &subject) {
    fs::file sdir = base.child("subjects/" + subject.identifier());

    std::vector<fs::file> res;
    std::copy_if(sdir.children().begin(), sdir.children().end(),
                 std::back_inserter(res), fs::fn_matcher("*.isoslant"));

    std::sort(res.begin(), res.end(), [](const fs::file &a, const fs::file &b) {
        return a.name() > b.name();
    });

    if (res.empty()) {
        throw std::runtime_error("Could not find isoslant for subject");
    }

    fs::file sfile = res.front();
    return yaml2isoslant(sfile.read_all());
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

std::map<uint8_t, uint16_t> store::lpm_leds() const {

    std::map<uint8_t, uint16_t> lpm_led_map;

    fs::file valuesFile = base.child("lpm/leds");
    std::string data = valuesFile.read_all();
    YAML::Node root = YAML::Load(data);

    YAML::Node led_node = root["leds"];

    for(YAML::const_iterator it = led_node.begin(); it != led_node.end(); it++) {
        std::string pin_no = it->first.as<std::string>();
        std::string led_wavelength = it->second.as<std::string>();

        uint8_t pin_int = std::stoi(pin_no);
        uint16_t pwm_val_int = std::stoi(led_wavelength);

        lpm_led_map.insert( std::pair<uint8_t ,uint16_t >(pin_int, pwm_val_int) );
    }

    std::cout << "Traversing the map; generated from LED PIN & Wavelength YAML Config File: " << std::endl;

    for(auto elem : lpm_led_map)    {
        std::cout << "pin: " << unsigned(elem.first) << "  --  Wavelength: " << unsigned(elem.second) << "\n";
    }

    return lpm_led_map;
}


// yaml stuff

fs::file store::cone_fundamentals(size_t spacing) const {
    std::stringstream fn;
    fn << "cones/sml_380@" << spacing << ".csv";
    return base.child(fn.str());
}

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
    calibration.gray_level = root["gray-level"].as<float>();

    std::string csv = root["dkl"].as<std::string>();
    calibration.dkl_params = dkl::parameter::from_csv_data(csv);
    calibration.dataset = root["dataset"].as<std::string>();

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


subject store::yaml2subject(const std::string &data) {
    YAML::Node doc = YAML::Load(data);
    YAML::Node root = doc["subject"];

    subject subject(root["id"].as<std::string>());
    subject.name = root["name"].as<std::string>();
    subject.initials = root["initials"].as<std::string>();

    return subject;
}

std::string store::subject2yaml(const subject &subject) {
    YAML::Emitter out;

    out << YAML::BeginMap;
    out << "subject";
    out << YAML::BeginMap;
    out << "id" << subject.identifier();

    out << "name" << subject.name;
    out << "initials" << subject.initials;

    out << YAML::EndMap;
    out << YAML::EndMap;

    return std::string(out.c_str());
}

isoslant store::yaml2isoslant(const std::string &data) {
    YAML::Node doc = YAML::Load(data);
    YAML::Node root = doc["isoslant"];

    isoslant iso(root["id"].as<std::string>());
    iso.subject = root["subject"].as<std::string>();
    iso.dl = root["dl"].as<double>();
    iso.phi = root["phi"].as<double>();
    iso.rgb2lms = root["rgb2lms"].as<std::string>();

    iso.display = yaml2display(root["display"]);
    return iso;
}

std::string store::isoslant2yaml(const isoslant &iso) {
    YAML::Emitter out;

    out << YAML::BeginMap;
    out << "isoslant";
    out << YAML::BeginMap;
    out << "id" << iso.identifier();

    out << "subject" << iso.subject;
    out << "dl" << iso.dl;
    out << "phi" << iso.phi;

    out << "display";
    emit_display(iso.display, out);
    out << "rgb2lms" << iso.rgb2lms;

    out << YAML::EndMap;
    out << YAML::EndMap;

    return std::string(out.c_str());
}


isodata store::yaml2isodata(const std::string &str) {
    typedef csv_iterator<std::string::const_iterator> csv_siterator;

    YAML::Node doc = YAML::Load(str);
    YAML::Node root = doc["isodata"];

    isodata d(root["id"].as<std::string>());
    d.subject = root["subject"].as<std::string>();
    d.display = yaml2display(root["display"]);
    d.rgb2lms = root["rgb2lms"].as<std::string>();

    std::string cd = root["data"].as<std::string>();

    bool is_header = true;
    for (auto iter = csv_siterator(cd.cbegin(), cd.cend(), ',');
         iter != csv_siterator();
         ++iter) {
        auto rec = *iter;

        if (rec.is_comment() || rec.is_empty()) {
            continue;
        }

        if (is_header) {
            is_header = false;
            continue;
        }

        if (rec.nfields() != 2) {
            throw std::runtime_error("Invalid CSV data for isodata::data");
        }

        float stimulus = rec.get_float(0);
        float response = rec.get_float(1);

        d.samples.emplace_back(stimulus, response);
    }

    return d;
}

std::string store::isodata2yaml(const isodata &data) {
    YAML::Emitter out;

    out << YAML::BeginMap;
    out << "isodata";
    out << YAML::BeginMap;
    out << "id" << data.identifier();
    out << "subject" << data.subject;

    out << "display";
    emit_display(data.display, out);
    out << "rgb2lms" << data.rgb2lms;

    out << "data" << YAML::Literal;

    std::stringstream cd;

    cd << "stimulus, response";
    for (const isodata::sample &s : data.samples) {
        cd << std::endl;
        cd << s.stimulus << ", " << s.response;
    }
    out << cd.str();

    out << YAML::EndMap;
    out << YAML::EndMap;

    return std::string(out.c_str());
}
} //iris::cfg::
} //iris::