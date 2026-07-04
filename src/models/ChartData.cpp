#include "ChartData.h"
#include <QUuid>
#include <QJsonArray>
#include <QJsonDocument>

// ─── createDefault ────────────────────────────────────────────────────────────

ChartData ChartData::createDefault(const QString& type) {
    ChartData d;
    d.type = type;

    if (type == "bar") {
        d.title = "Quarterly Comparison";
        d.labels = {"Q1", "Q2", "Q3", "Q4"};
        d.series = {
            {"Revenue",  {42.0, 67.0, 53.0, 89.0}, "#4e79a7", {}},
            {"Profit",  {18.0, 28.0, 21.0, 37.0}, "#f28e2b", {}}
        };

    } else if (type == "bar_h") {
        d.title = "Product Comparison";
        d.labels = {"Product A", "Product B", "Product C", "Product D"};
        d.series = {{"Revenue", {85.0, 63.0, 91.0, 47.0}, "#4e79a7", {}}};

    } else if (type == "line") {
        d.title = "Monthly Trend";
        d.labels = {"Jan", "Feb", "Mar", "Apr", "May", "Jun"};
        d.series = {
            {"Plan",  {30.0, 35.0, 38.0, 42.0, 45.0, 50.0}, "#4e79a7", {}},
            {"Actual",   {28.0, 33.0, 41.0, 38.0, 47.0, 52.0}, "#e15759", {}}
        };

    } else if (type == "area") {
        d.title = "Revenue Trend";
        d.labels = {"Jan", "Feb", "Mar", "Apr", "May", "Jun"};
        d.series = {
            {"Online",   {12.0, 19.0, 15.0, 25.0, 22.0, 30.0}, "#4e79a7", {}},
            {"In-Store",  { 8.0, 12.0, 18.0, 14.0, 26.0, 24.0}, "#59a14f", {}}
        };

    } else if (type == "pie") {
        d.title = "Sales Channels";
        d.showGrid = false;
        d.labels = {"Direct Sales", "Online", "Partner", "Other"};
        d.series = {{"Share", {35.0, 28.0, 22.0, 15.0}, "", {}}};

    } else if (type == "donut") {
        d.title = "Market Share";
        d.showGrid = false;
        d.labels = {"Own Brand", "Competitor A", "Competitor B", "Other"};
        d.series = {{"Share", {40.0, 30.0, 20.0, 10.0}, "", {}}};

    } else if (type == "scatter") {
        d.title = "Correlation Chart";
        d.labels = {};
        d.series = {{
            "Data",
            {10.0,20.0, 15.0,35.0, 25.0,15.0, 30.0,45.0, 40.0,30.0, 50.0,55.0},
            "#4e79a7", {}
        }};

    } else if (type == "flowchart") {
        d.title = "Process Flow";
        auto mkNode = [](const QString& id, const QString& lbl, const QString& shape,
                         const QString& col, double x, double y, double w, double h) {
            ChartNode n; n.id=id; n.label=lbl; n.shape=shape;
            n.color=col; n.x=x; n.y=y; n.w=w; n.h=h;
            return n;
        };
        d.nodes = {
            mkNode("n1","Start","oval",      "#59a14f",400,30, 200,60),
            mkNode("n2","Validate Input","rect", "#4e79a7",370,150,260,60),
            mkNode("n3","Valid?","diamond", "#edc948",400,280,200,80),
            mkNode("n4","Process","rect","#4e79a7",600,420,220,60),
            mkNode("n5","Report Error","rect","#e15759",160,420,220,60),
            mkNode("n6","End","oval",       "#59a14f",400,550,200,60),
        };
        d.nodes[0].edges = {"n2"};
        d.nodes[1].edges = {"n3"};
        d.nodes[2].edges = {"n4","n5"};
        d.nodes[3].edges = {"n6"};
        d.nodes[4].edges = {"n6"};

    } else if (type == "mindmap") {
        d.title = "Mind Map";
        auto mkNode = [](const QString& id, const QString& lbl, const QString& col) {
            ChartNode n; n.id=id; n.label=lbl; n.color=col; return n;
        };
        d.nodes = {
            mkNode("root","Main Topic",  "#4e79a7"),
            mkNode("a",  "Idea A",       "#f28e2b"),
            mkNode("b",  "Idea B",       "#e15759"),
            mkNode("c",  "Idea C",       "#59a14f"),
            mkNode("a1", "Point A1",     "#f28e2b"),
            mkNode("a2", "Point A2",     "#f28e2b"),
            mkNode("b1", "Point B1",     "#e15759"),
            mkNode("c1", "Point C1",     "#59a14f"),
            mkNode("c2", "Point C2",     "#59a14f"),
        };
        d.nodes[0].edges = {"a","b","c"};
        d.nodes[1].edges = {"a1","a2"};
        d.nodes[2].edges = {"b1"};
        d.nodes[3].edges = {"c1","c2"};

    } else if (type == "orgchart") {
        d.title = "Org Chart";
        auto mkNode = [](const QString& id, const QString& lbl, const QString& col) {
            ChartNode n; n.id=id; n.label=lbl; n.color=col;
            n.w=160; n.h=55; return n;
        };
        d.nodes = {
            mkNode("ceo",    "CEO",         "#4e79a7"),
            mkNode("cto",    "CTO",         "#f28e2b"),
            mkNode("cfo",    "CFO",         "#e15759"),
            mkNode("coo",    "COO",         "#59a14f"),
            mkNode("dev",    "Dev Lead",    "#f28e2b"),
            mkNode("fin",    "Finance",     "#e15759"),
            mkNode("ops",    "Operations",  "#59a14f"),
        };
        d.nodes[0].edges = {"cto","cfo","coo"};
        d.nodes[1].edges = {"dev"};
        d.nodes[2].edges = {"fin"};
        d.nodes[3].edges = {"ops"};

    } else if (type == "timeline") {
        d.title = "Company History";
        d.events = {
            {"Founding",      "Initial Ideas",          0.0,  "#4e79a7"},
            {"Prototype",     "First Prototype",       25.0, "#f28e2b"},
            {"Launch",        "Market Launch",       50.0, "#59a14f"},
            {"Expansion",     "Internationalization", 75.0, "#e15759"},
            {"Today",         "Current Position",    100.0, "#b07aa1"},
        };

    } else if (type == "gantt") {
        d.title = "Project Plan";
        d.tasks = {
            {"Concept",      0.0,  20.0, "#4e79a7"},
            {"Design",      15.0,  40.0, "#f28e2b"},
            {"Development", 35.0,  75.0, "#e15759"},
            {"Testing",     65.0,  90.0, "#59a14f"},
            {"Launch",      85.0, 100.0, "#b07aa1"},
        };
        d.ganttAxisLabels = {"Jan","Feb","Mar","Apr","May"};

    } else if (type == "uml") {
        d.title = "Class Diagram";
        auto mkClass = [](const QString& id, const QString& lbl,
                          double x, double y, double w, double h) {
            ChartNode n; n.id=id; n.label=lbl; n.shape="class";
            n.x=x; n.y=y; n.w=w; n.h=h; return n;
        };
        d.nodes = {
            mkClass("Animal","Animal|+ name: String\n+ age: int|+ eat(): void\n+ sleep(): void",
                    310,30,300,130),
            mkClass("Dog","Dog|+ breed: String|+ bark(): void\n+ fetch(): void",
                    100,350,260,120),
            mkClass("Cat","Cat|+ color: String|+ purr(): void",
                    540,350,240,100),
        };
        d.nodes[1].edges = {"Animal|inheritance"};
        d.nodes[2].edges = {"Animal|inheritance"};

    } else if (type == "venn") {
        d.title = "Intersection Diagram";
        d.showGrid = false;
        d.vennCircles = {
            {"Set A", 35.0, 50.0, 32.0, "#4e79a7", 0.50},
            {"Set B", 65.0, 50.0, 32.0, "#e15759", 0.50},
        };
    }

    return d;
}

// ─── JSON serialisation ───────────────────────────────────────────────────────

static QJsonArray strListToArr(const QStringList& sl) {
    QJsonArray a; for (const auto& s : sl) a.append(s); return a;
}
static QStringList arrToStrList(const QJsonArray& a) {
    QStringList sl; for (const auto& v : a) sl << v.toString(); return sl;
}
static QJsonArray dblVecToArr(const QVector<double>& v) {
    QJsonArray a; for (double d : v) a.append(d); return a;
}
static QVector<double> arrToDblVec(const QJsonArray& a) {
    QVector<double> v; for (const auto& x : a) v << x.toDouble(); return v;
}

QJsonObject ChartData::toJson() const {
    QJsonObject o;
    o["type"]        = type;
    o["title"]       = title;
    o["description"] = description;
    o["showLegend"]  = showLegend;
    o["showGrid"]    = showGrid;
    o["labels"]      = strListToArr(labels);

    QJsonArray serArr;
    for (const auto& s : series) {
        QJsonObject so;
        so["name"]        = s.name;
        so["values"]      = dblVecToArr(s.values);
        so["color"]       = s.color;
        so["valueColors"] = strListToArr(s.valueColors);
        serArr.append(so);
    }
    o["series"] = serArr;

    QJsonArray nodeArr;
    for (const auto& n : nodes) {
        QJsonObject no;
        no["id"]    = n.id;  no["label"] = n.label;
        no["shape"] = n.shape; no["color"] = n.color;
        no["edges"] = strListToArr(n.edges);
        no["x"] = n.x; no["y"] = n.y; no["w"] = n.w; no["h"] = n.h;
        nodeArr.append(no);
    }
    o["nodes"] = nodeArr;

    QJsonArray evArr;
    for (const auto& ev : events) {
        QJsonObject eo;
        eo["label"] = ev.label; eo["desc"] = ev.desc;
        eo["pos"]   = ev.pos;   eo["color"] = ev.color;
        evArr.append(eo);
    }
    o["events"] = evArr;

    QJsonArray taskArr;
    for (const auto& t : tasks) {
        QJsonObject to;
        to["name"]  = t.name;  to["color"] = t.color;
        to["start"] = t.start; to["end"]   = t.end;
        taskArr.append(to);
    }
    o["tasks"]          = taskArr;
    o["ganttAxisLabels"] = strListToArr(ganttAxisLabels);

    QJsonArray vArr;
    for (const auto& v : vennCircles) {
        QJsonObject vo;
        vo["label"]   = v.label;  vo["color"]   = v.color;
        vo["cx"]      = v.cx;     vo["cy"]       = v.cy;
        vo["radius"]  = v.radius; vo["opacity"]  = v.opacity;
        vArr.append(vo);
    }
    o["vennCircles"] = vArr;
    return o;
}

ChartData ChartData::fromJson(const QJsonObject& o) {
    ChartData d;
    d.type        = o["type"].toString("bar");
    d.title       = o["title"].toString();
    d.description = o["description"].toString();
    d.showLegend  = o["showLegend"].toBool(true);
    d.showGrid    = o["showGrid"].toBool(true);
    d.labels      = arrToStrList(o["labels"].toArray());

    for (const auto& sv : o["series"].toArray()) {
        QJsonObject so = sv.toObject();
        ChartSeries s;
        s.name        = so["name"].toString();
        s.values      = arrToDblVec(so["values"].toArray());
        s.color       = so["color"].toString();
        s.valueColors = arrToStrList(so["valueColors"].toArray());
        d.series << s;
    }
    for (const auto& nv : o["nodes"].toArray()) {
        QJsonObject no = nv.toObject();
        ChartNode n;
        n.id    = no["id"].toString();  n.label = no["label"].toString();
        n.shape = no["shape"].toString("rect"); n.color = no["color"].toString();
        n.edges = arrToStrList(no["edges"].toArray());
        n.x = no["x"].toDouble(); n.y = no["y"].toDouble();
        n.w = no["w"].toDouble(150); n.h = no["h"].toDouble(60);
        d.nodes << n;
    }
    for (const auto& ev : o["events"].toArray()) {
        QJsonObject eo = ev.toObject();
        ChartTimelineEvent e;
        e.label = eo["label"].toString(); e.desc  = eo["desc"].toString();
        e.pos   = eo["pos"].toDouble(50); e.color = eo["color"].toString();
        d.events << e;
    }
    for (const auto& tv : o["tasks"].toArray()) {
        QJsonObject to = tv.toObject();
        ChartGanttTask t;
        t.name  = to["name"].toString();  t.color = to["color"].toString();
        t.start = to["start"].toDouble(); t.end   = to["end"].toDouble(50);
        d.tasks << t;
    }
    d.ganttAxisLabels = arrToStrList(o["ganttAxisLabels"].toArray());

    for (const auto& vv : o["vennCircles"].toArray()) {
        QJsonObject vo = vv.toObject();
        ChartVennCircle v;
        v.label   = vo["label"].toString();  v.color   = vo["color"].toString();
        v.cx      = vo["cx"].toDouble(50);   v.cy      = vo["cy"].toDouble(50);
        v.radius  = vo["radius"].toDouble(30); v.opacity = vo["opacity"].toDouble(0.45);
        d.vennCircles << v;
    }
    return d;
}
