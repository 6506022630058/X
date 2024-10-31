from flask import Flask, request, jsonify, render_template
import asyncio
from pysnmp.hlapi.v3arch.asyncio import *

app = Flask(__name__)

@app.route('/')
def index():
    return render_template('index.html')  # Render the HTML template

@app.route('/snmp/control/<ip>/<community>/<port>', methods=['POST'])
async def control_port(ip, community, port):
    oid_ifAdminStatus = f'1.3.6.1.2.1.2.2.1.7.{port}'  # OID for ifAdminStatus
    status = request.json.get('status')
    if status is None:
        return jsonify({"error": "Status not provided"}), 400

    new_status = 1 if status == 'up' else 2  # 1 for up, 2 for down

    errorIndication, errorStatus, errorIndex, varBinds = await set_cmd(
        SnmpEngine(),
        CommunityData(community),
        await UdpTransportTarget.create((ip, 161)),
        ContextData(),
        ObjectType(ObjectIdentity(oid_ifAdminStatus), Integer(new_status))
    )

    if errorIndication:
        return jsonify({"error": str(errorIndication)}), 400
    elif errorStatus:
        return jsonify({"error": str(errorStatus.prettyPrint())}), 400
    else:
        return jsonify({"message": "Port status updated successfully."})

@app.route('/snmp/ports/<ip>/<community>', methods=['GET'])
async def get_ports(ip, community):
    oid_ifTable = '1.3.6.1.2.1.2.2.1.7'
    oid_ifDescr = '1.3.6.1.2.1.2.2.1.2'

    errorIndication, errorStatus, errorIndex, varBinds_admin = await bulk_cmd(
        SnmpEngine(),
        CommunityData(community),
        await UdpTransportTarget.create((ip, 161)),
        ContextData(),
        0, 10,
        ObjectType(ObjectIdentity(oid_ifTable))
    )

    errorIndication_descr, errorStatus_descr, errorIndex_descr, varBinds_descr = await bulk_cmd(
        SnmpEngine(),
        CommunityData(community),
        await UdpTransportTarget.create((ip, 161)),
        ContextData(),
        0, 10,
        ObjectType(ObjectIdentity(oid_ifDescr))
    )

    if errorIndication or errorIndication_descr:
        return jsonify({"error": "Failed to retrieve ports"}), 400
    else:
        results = {}
        port_names = {str(name).split('.')[-1]: str(value) for name, value in varBinds_descr}
        for name, value in varBinds_admin:
            port_index = str(name).split('.')[-1]
            results[port_index] = {
                "status": str(value),
                "name": port_names.get(port_index, "Unknown Port")
            }
        return jsonify(results)

@app.route('/snmp/device-name/<ip>/<community>', methods=['GET'])
async def get_device_name(ip, community):
    oid_sysName = '1.3.6.1.2.1.1.5.0'
    errorIndication, errorStatus, errorIndex, varBinds = await get_cmd(
        SnmpEngine(),
        CommunityData(community),
        await UdpTransportTarget.create((ip, 161)),
        ContextData(),
        ObjectType(ObjectIdentity(oid_sysName))
    )

    if errorIndication:
        return jsonify({"error": str(errorIndication)}), 400
    elif errorStatus:
        return jsonify({"error": str(errorStatus.prettyPrint())}), 400
    else:
        return jsonify({"device_name": str(varBinds[0][1])})

@app.route('/snmp/uptime/<ip>/<community>', methods=['GET'])
async def get_uptime(ip, community):
    oid_sysUpTime = '1.3.6.1.2.1.1.3.0'
    errorIndication, errorStatus, errorIndex, varBinds = await get_cmd(
        SnmpEngine(),
        CommunityData(community),
        await UdpTransportTarget.create((ip, 161)),
        ContextData(),
        ObjectType(ObjectIdentity(oid_sysUpTime))
    )

    if errorIndication:
        return jsonify({"error": str(errorIndication)}), 400
    elif errorStatus:
        return jsonify({"error": str(errorStatus.prettyPrint())}), 400
    else:
        uptime = int(varBinds[0][1]) / 100  # Convert to seconds
        days = uptime // 86400
        hours = (uptime % 86400) // 3600
        minutes = (uptime % 3600) // 60
        seconds = uptime % 60
        return jsonify({"uptime": f"{int(days)}d {int(hours)}h {int(minutes)}m {int(seconds)}s"})

@app.route('/snmp/custom-oid/<ip>/<community>/<oid>', methods=['GET'])
async def get_custom_oid(ip, community, oid):
    errorIndication, errorStatus, errorIndex, varBinds = await get_cmd(
        SnmpEngine(),
        CommunityData(community),
        await UdpTransportTarget.create((ip, 161)),
        ContextData(),
        ObjectType(ObjectIdentity(oid))
    )

    if errorIndication:
        return jsonify({"error": str(errorIndication)}), 400
    elif errorStatus:
        return jsonify({"error": str(errorStatus.prettyPrint())}), 400
    else:
        return jsonify({"value": str(varBinds[0][1])})

if __name__ == '__main__':
    app.run(debug=True)