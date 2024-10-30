from flask import Flask, render_template
from flask_socketio import SocketIO, emit
import asyncio
from pysnmp.hlapi.v3arch.asyncio import get_cmd, set_cmd, SnmpEngine, CommunityData, UdpTransportTarget, ContextData, ObjectType, ObjectIdentity

app = Flask(__name__)
app.config['SECRET_KEY'] = 'secret!'
socketio = SocketIO(app)

interface_data = {}

@app.route('/')
def index():
    return render_template('index.html')

@socketio.on('get_snmp_info')
def handle_get_snmp_info(data):
    ip = data['ip']
    ro = data['ro']
    rw = data['rw']
    if validate_ip(ip):
        asyncio.run(fetch_interfaces(ip, ro, rw))
    else:
        emit('update_interface_data', {'error': 'Invalid IP Address'})

@socketio.on('toggle_interface_status')
def handle_toggle_interface_status(data):
    ip = data['ip']
    rw = data['rw']
    index = data['index']
    current_status = data['current_status']
    new_status = '2' if current_status == '1' else '1'
    
    if validate_ip(ip):
        asyncio.run(set_interface_status(ip, rw, index, new_status))
        asyncio.run(fetch_interfaces(ip, data['ro'], rw))
    else:
        emit('update_interface_data', {'error': 'Invalid IP Address'})

@socketio.on('get_oid_value')
def handle_get_oid_value(data):
    ip = data['ip']
    ro = data['ro']
    oid = data['oid']
    
    if validate_ip(ip):
        value = asyncio.run(fetch_sys_info(ip, ro, oid))
        emit('update_oid_value', {'oid': oid, 'value': value})
    else:
        emit('update_oid_value', {'error': 'Invalid IP Address'})

async def fetch_interfaces(ip, ro, rw):
    global interface_data
    interface_data = {}
    index = 1

    interface_data['sysName'] = await fetch_sys_info(ip, ro, '1.3.6.1.2.1.1.5.0')
    interface_data['sysDescr'] = await fetch_sys_info(ip, ro, '1.3.6.1.2.1.1.1.0')

    try:
        while True:
            interface_name = await fetch_interface_info(ip, ro, f'1.3.6.1.2.1.2.2.1.2.{index}')
            if not interface_name:
                break
            
            admin_status = await fetch_interface_info(ip, ro, f'1.3.6.1.2.1.2.2.1.7.{index}')
            interface_data[f'interface_{index}'] = {
                'name': interface_name,
                'admin_status': admin_status,
                'index': index
            }
            index += 1

    except Exception as e:
        print(f"Error fetching interfaces: {e}")
    
    emit('update_interface_data', interface_data)

async def fetch_sys_info(ip, community, oid):
    errorIndication, errorStatus, errorIndex, varBinds = await get_cmd(
        SnmpEngine(),
        CommunityData(community),
        await UdpTransportTarget.create((ip, 161)),
        ContextData(),
        ObjectType(ObjectIdentity(oid))
    )
    return str(varBinds[0][1]) if not errorIndication and not errorStatus else None

async def fetch_interface_info(ip, community, oid):
    errorIndication, errorStatus, errorIndex, varBinds = await get_cmd(
        SnmpEngine(),
        CommunityData(community),
        await UdpTransportTarget.create((ip, 161)),
        ContextData(),
        ObjectType(ObjectIdentity(oid))
    )
    return str(varBinds[0][1]) if not errorIndication and not errorStatus else None

async def set_interface_status(ip, community, index, status):
    errorIndication, errorStatus, errorIndex, varBinds = await set_cmd(
        SnmpEngine(),
        CommunityData(community, mpModel=0),
        await UdpTransportTarget.create((ip, 161)),
        ContextData(),
        ObjectType(ObjectIdentity(f'1.3.6.1.2.1.2.2.1.7.{index}'), status)
    )
    if errorIndication or errorStatus:
        print(f"Error setting interface status: {errorIndication or errorStatus}")

def validate_ip(ip):
    try:
        parts = ip.split('.')
        if len(parts) != 4:
            return False
        for part in parts:
            if not part.isdigit() or not (0 <= int(part) <= 255):
                return False
        return True
    except Exception:
        return False

if __name__ == '__main__':
    socketio.run(app, debug=True, allow_unsafe_werkzeug=True)
