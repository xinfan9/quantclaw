const http = require('http');

const port = process.env.QUANTCLAW_SIDECAR_PORT || 18802;

const tools = [
  {
    name: 'datetime',
    description: 'Get the current date and time in ISO 8601 format.',
    parameters: { type: 'object', properties: {} }
  },
  {
    name: 'echo',
    description: 'Echo the input message back.',
    parameters: {
      type: 'object',
      properties: {
        message: {
          type: 'string',
          description: 'The message to echo.'
        }
      },
      required: ['message']
    }
  }
];

const server = http.createServer((req, res) => {
  res.setHeader('Content-Type', 'application/json');
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

  if (req.method === 'OPTIONS') {
    res.statusCode = 204;
    res.end();
    return;
  }

  if (req.url === '/tools' && req.method === 'GET') {
    res.end(JSON.stringify({ tools }));
    return;
  }

  if (req.url === '/call' && req.method === 'POST') {
    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', () => {
      try {
        const { name, arguments: args } = JSON.parse(body);
        if (name === 'datetime') {
          res.end(JSON.stringify({ result: new Date().toISOString() }));
        } else if (name === 'echo') {
          const msg = (args && args.message) ? args.message : '';
          res.end(JSON.stringify({ result: `Echo: ${msg}` }));
        } else {
          res.statusCode = 404;
          res.end(JSON.stringify({ error: 'Tool not found' }));
        }
      } catch (e) {
        res.statusCode = 400;
        res.end(JSON.stringify({ error: e.message }));
      }
    });
    return;
  }

  res.statusCode = 404;
  res.end(JSON.stringify({ error: 'Not found' }));
});

server.listen(port, '127.0.0.1', () => {
  console.log(`QuantClaw sidecar listening on http://127.0.0.1:${port}`);
});
