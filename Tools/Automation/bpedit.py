"""Small client for editing Blueprint graphs through the Unreal MCP server."""
import json
import sys
import urllib.request

URL = "http://localhost:13579/mcp"


def call(tool, **args):
    body = json.dumps({"jsonrpc": "2.0", "id": 1, "method": "tools/call",
                       "params": {"name": tool, "arguments": args}}).encode("utf-8")
    req = urllib.request.Request(URL, data=body, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=300) as resp:
        data = json.loads(resp.read().decode("utf-8"))
    if "error" in data:
        raise RuntimeError(f"{tool}: {data['error']}")
    result = data["result"]
    text = result["content"][0].get("text", "")
    if result.get("isError"):
        raise RuntimeError(f"{tool}: {text}")
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        return text


def nodes(asset, graph):
    return call("get_node_pins", asset_path=asset, graph_name=graph)["nodes"]


def find(node_list, name, nth=0):
    hits = [n for n in node_list if name in n["name"]]
    if len(hits) <= nth:
        raise KeyError(f"no node named like {name!r}; have {[n['name'] for n in node_list]}")
    return hits[nth]


def pin(node, name):
    for p in node["pins"]:
        if p["name"] == name:
            return p
    raise KeyError(f"node {node['name']} has no pin {name}: {[p['name'] for p in node['pins']]}")


def link(asset, graph, src, src_pin, dst, dst_pin):
    return call("connect_pins", asset_path=asset, graph_name=graph,
                source_node_id=src["id"] if isinstance(src, dict) else src, source_pin_name=src_pin,
                target_node_id=dst["id"] if isinstance(dst, dict) else dst, target_pin_name=dst_pin)


def unlink(asset, graph, node, pin_name):
    return call("disconnect_pin", asset_path=asset, graph_name=graph,
                node_id=node["id"] if isinstance(node, dict) else node, pin_name=pin_name)


def add_call(asset, graph, function, target, x=0, y=0):
    return call("add_function_call_node", asset_path=asset, graph_name=graph,
                function_name=function, target=target, node_x=x, node_y=y)


def dump(asset, graph):
    for n in nodes(asset, graph):
        links = []
        for p in n["pins"]:
            if p.get("connections"):
                links.append(f"{p['name']}->" + ",".join(c["nodeId"][:6] + "." + c["pinName"] for c in p["connections"]))
        print(n["id"][:6], n["name"].replace("\n", " | ")[:50], "|", "; ".join(links)[:200])


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    dump(sys.argv[1], sys.argv[2])
