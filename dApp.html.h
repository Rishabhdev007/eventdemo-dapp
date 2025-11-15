<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width,initial-scale=1" />
    <title>EventDemo — Simple dApp</title>
    <style>
      :root{font-family:system-ui,-apple-system,Segoe UI,Roboto,'Helvetica Neue',Arial}
      body{max-width:900px;margin:28px auto;padding:18px}
      header{display:flex;align-items:center;justify-content:space-between}
      h1{font-size:20px;margin:0}
      .card{border-radius:12px;padding:14px;margin-top:14px;box-shadow:0 6px 18px rgba(0,0,0,0.06)}
      .row{display:flex;gap:8px;align-items:center}
      input[type=text]{flex:1;padding:10px;border-radius:8px;border:1px solid #ddd}
      button{padding:10px 14px;border-radius:8px;border:0;cursor:pointer}
      button.primary{background:#111;color:#fff}
      button.ghost{background:transparent;border:1px solid #ccc}
      pre{white-space:pre-wrap;background:#f8f9fa;padding:10px;border-radius:8px;border:1px solid #eee}
      .events{max-height:280px;overflow:auto;margin-top:10px}
      .evt{padding:8px;border-bottom:1px dashed #eee}
      .small{font-size:13px;color:#555}
    </style>
    <!-- Ethers v6 browser build -->
    <script src="https://cdn.jsdelivr.net/npm/ethers@6/dist/ethers.min.js"></script>
  </head>
  <body>
    <header>
      <h1>EventDemo — dApp (Sepolia)</h1>
      <div id="walletStatus" class="small">Disconnected</div>
    </header>

    <section class="card">
      <div class="row" style="margin-bottom:10px">
        <button id="connectBtn" class="primary">Connect Wallet</button>
        <button id="refreshEvents" class="ghost">Refresh Events</button>
        <button id="clearEvents" class="ghost">Clear Events</button>
      </div>

      <div>
        <div class="small">Contract:</div>
        <pre id="contractAddr">0xE3e7fc9b468519B911c114F377C6d45e21Ac4284</pre>
      </div>

      <div style="margin-top:12px">
        <div class="small">Call ping() — emits ActionLogged(msg.sender, "Ping called!", timestamp)</div>
        <div style="margin-top:8px">
          <button id="pingBtn" class="primary">Send ping()</button>
        </div>
      </div>

      <div style="margin-top:14px">
        <div class="small">Call setMessage(string)</div>
        <div class="row" style="margin-top:8px">
          <input id="msgInput" type="text" placeholder="Type message to set" />
          <button id="setMsgBtn" class="primary">setMessage()</button>
        </div>
      </div>

      <div style="margin-top:14px">
        <div class="small">Latest on-chain message (read):</div>
        <pre id="currentMessage">(not read yet)</pre>
        <button id="readMsg" class="ghost" style="margin-top:8px">Refresh message</button>
      </div>
    </section>

    <section class="card">
      <div class="small">Event logs (ActionLogged)</div>
      <div class="events" id="events"></div>
    </section>

    <section style="margin-top:14px" class="small">
      Tips: Make sure MetaMask is on Sepolia and unlocked. Approve the transactions when MetaMask pops up.
    </section>

    <script>
      (async function(){
        // Minimal ABI for EventDemo
        const abi = [
          "event ActionLogged(address indexed user, string message, uint256 timestamp)",
          "function ping() public",
          "function setMessage(string _msg) public",
          "function message() public view returns (string)"
        ];

        const contractAddress = document.getElementById('contractAddr').innerText.trim();
        let provider, signer, contract;

        const connectBtn = document.getElementById('connectBtn');
        const walletStatus = document.getElementById('walletStatus');
        const pingBtn = document.getElementById('pingBtn');
        const setMsgBtn = document.getElementById('setMsgBtn');
        const msgInput = document.getElementById('msgInput');
        const eventsDiv = document.getElementById('events');
        const readMsgBtn = document.getElementById('readMsg');
        const currentMessage = document.getElementById('currentMessage');
        const refreshEventsBtn = document.getElementById('refreshEvents');
        const clearEventsBtn = document.getElementById('clearEvents');

        function shortAddr(a){ return a.slice(0,6)+"..."+a.slice(-4); }

        function addEvent(e){
          const el = document.createElement('div');
          el.className = 'evt';
          const time = new Date(Number(e.timestamp) * 1000).toLocaleString();
          el.innerHTML = `<div><strong>${e.message}</strong></div><div class="small">by ${shortAddr(e.user)} • ${time}</div>`;
          eventsDiv.prepend(el);
        }

        async function attachContract(){
          provider = new ethers.BrowserProvider(window.ethereum);
          signer = await provider.getSigner();
          contract = new ethers.Contract(contractAddress, abi, signer);
        }

        async function connectWallet(){
          if(!window.ethereum) return alert('MetaMask not found');
          try{
            await window.ethereum.request({ method: 'eth_requestAccounts' });
            await attachContract();
            const acc = await signer.getAddress();
            walletStatus.innerText = 'Connected: ' + shortAddr(acc);
            connectBtn.innerText = 'Connected';
            connectBtn.disabled = true;
            // start listening for events
            listenEvents();
            readCurrentMessage();
          }catch(err){
            console.error(err);
            alert('Connection rejected or failed.');
          }
        }

        async function sendPing(){
          try{
            const tx = await contract.ping();
            connectBtn.disabled = true;
            pingBtn.disabled = true;
            const receipt = await tx.wait();
            alert('ping tx mined: ' + receipt.transactionHash);
            readCurrentMessage();
            pingBtn.disabled = false;
            connectBtn.disabled = false;
          }catch(err){
            console.error(err);
            alert('Transaction failed: ' + (err && err.message));
            pingBtn.disabled = false;
          }
        }

        async function doSetMessage(){
          const txt = msgInput.value.trim();
          if(!txt) return alert('Enter a message first');
          try{
            setMsgBtn.disabled = true;
            const tx = await contract.setMessage(txt);
            const receipt = await tx.wait();
            alert('setMessage tx mined: ' + receipt.transactionHash);
            msgInput.value = '';
            readCurrentMessage();
            setMsgBtn.disabled = false;
          }catch(err){
            console.error(err);
            alert('Transaction failed: ' + (err && err.message));
            setMsgBtn.disabled = false;
          }
        }

        async function readCurrentMessage(){
          try{
            const m = await contract.message();
            currentMessage.innerText = m || '(empty)';
          }catch(err){
            console.error(err);
            currentMessage.innerText = '(read failed)';
          }
        }

        async function fetchPastEvents(){
          // Use provider's getLogs with the event topic
          try{
            const iface = new ethers.Interface(abi);
            const topic = iface.getEventTopic('ActionLogged');
            const filter = { address: contractAddress, topics: [topic] };
            const logs = await provider.getLogs(filter);
            for(const log of logs.reverse()){
              const parsed = iface.parseLog(log);
              addEvent({ user: parsed.args.user, message: parsed.args.message, timestamp: parsed.args.timestamp.toString() });
            }
          }catch(err){
            console.error('fetch past', err);
          }
        }

        function listenEvents(){
          try{
            const iface = new ethers.Interface(abi);
            // remove previous listeners to avoid duplicates
            provider.off && provider.off(contractAddress);
            contract.on && contract.on('ActionLogged', (user, message, timestamp, event) => {
              addEvent({ user, message, timestamp: timestamp.toString() });
            });
          }catch(err){
            console.error('listen', err);
          }
        }

        // UI hooks
        connectBtn.addEventListener('click', connectWallet);
        pingBtn.addEventListener('click', sendPing);
        setMsgBtn.addEventListener('click', doSetMessage);
        readMsgBtn.addEventListener('click', readCurrentMessage);
        refreshEventsBtn.addEventListener('click', () => { eventsDiv.innerHTML = ''; fetchPastEvents(); });
        clearEventsBtn.addEventListener('click', () => { eventsDiv.innerHTML = ''; });

        // Auto-attach provider if already available
        if(window.ethereum && window.ethereum.selectedAddress){
          connectWallet().catch(()=>{});
        }

      })();
    </script>
  </body>
</html>
