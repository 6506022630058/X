from flask import Flask, render_template, request, redirect, url_for, flash
import paramiko, threading, time, io

app = Flask(__name__)
app.secret_key = 'your_secret_key'
ssh_client, shell, output_buffer, interfaces = None, None, io.StringIO(), []

def ssh_connect(ip, username, password):
    global ssh_client, shell, output_buffer, interfaces
    ssh_client = paramiko.SSHClient()
    ssh_client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    
    ssh_client.connect(ip, username=username, password=password, timeout=10)
    
    shell = ssh_client.invoke_shell()
    shell.send("terminal length 0\n")
    time.sleep(1)  
    
    shell.send("sh ip int b\n")
    time.sleep(1)  

    while shell.recv_ready():
        output = shell.recv(1024).decode()
        output_buffer.write(output)
        
        interfaces += [line.split()[0] for line in output.splitlines() if line and not line.startswith('Interface') and len(line.split()) >= 6]

@app.route('/', methods=['GET', 'POST'])
def index():
    if request.method == 'POST':
        ip, username, password = request.form['ip'], request.form['username'], request.form['password']
        if not ip or not username or not password:
            flash('Please fill in all fields.')
            return redirect(url_for('index'))
        threading.Thread(target=ssh_connect, args=(ip, username, password)).start()
        return redirect(url_for('terminal'))
    return render_template('index.html')

@app.route('/terminal')
def terminal():
    return render_template('terminal.html', output=output_buffer.getvalue())

@app.route('/execute', methods=['POST'])
def execute():
    global shell, output_buffer
    command = request.form['command']
    shell.send(f"{command}\n")
    time.sleep(1)
    while shell.recv_ready():
        output_buffer.write(shell.recv(1024).decode())
    return redirect(url_for('terminal'))

@app.route('/config', methods=['GET', 'POST'])
def config():
    if request.method == 'POST':
        form_type = request.form.get('form_type')
        interface = request.form.get('interface')

        if form_type == 'hostname':
            shell.send(f"en\nconf t\nhostname {request.form['hostname']}\nend\n")
            output_buffer.write(f"Configured hostname to {request.form['hostname']}\n")
        
        elif form_type == 'ip_address' and interface and interface != "Select Interface":
            shell.send(f"en\nconf t\ninterface {interface}\n")

            if request.form.get('no_switchport') == '1':
                shell.send("no switchport\n")
                output_buffer.write(f"Configured {interface} as no switchport\n")
            else:output_buffer.write(f"Configured {interface} as switchport\n")

            shell.send(f"ip address {request.form['ipv4']} {request.form['netmask']}\nend\n")
            output_buffer.write(f"Configured {interface} with IP {request.form['ipv4']}/{request.form['netmask']}\n")
        
        elif form_type == 'shutdown' and interface and interface != "Select Interface":
            shell.send(f"en\nconf t\ninterface {interface}\nshutdown\nend\n")
            output_buffer.write(f"shutdown applied to {interface}\n")
        
        elif form_type == 'noshutdown' and interface and interface != "Select Interface":
            shell.send(f"en\nconf t\ninterface {interface}\nno shutdown\nend\n")
            output_buffer.write(f"no shutdown applied to {interface}\n")
        
        return redirect(url_for('terminal'))
    
    return render_template('config.html', interfaces=interfaces)

@app.route('/clear_output', methods=['POST'])
def clear_output():
    global output_buffer
    output_buffer = io.StringIO()
    return redirect(url_for('terminal'))

if __name__ == '__main__':
    app.run(debug=True)