'use strict';

const READ_METHODS = new Set(['chain.get_chain_id', 'chain.get_head_info',
  'chain.read_contract', 'chain.get_account_rc', 'block_store.get_blocks_by_height']);

function createReadRpc({ endpoint, fetchImpl = globalThis.fetch,
  sleep = ms => new Promise(resolve => setTimeout(resolve, ms)), random = Math.random,
  attempts = 5, timeoutMs = 15000, intervalMs = 500, now = Date.now,
  warn = message => console.error(message) }) {
  const url = new URL(endpoint);
  if (!['http:', 'https:'].includes(url.protocol) || url.username || url.password) throw new Error('Invalid RPC URL');
  let id = 0, nextRequest = 0;
  return async function call(method, params) {
    if (!READ_METHODS.has(method)) throw new Error(`Read-only RPC transport refuses ${method}`);
    const requestId = ++id;
    for (let attempt = 0; attempt < attempts; attempt++) {
      await sleep(Math.max(0, nextRequest - now()));
      nextRequest = now() + intervalMs;
      const controller = new AbortController();
      const timer = setTimeout(() => controller.abort(), timeoutMs);
      let retryable = false, retryAfter = 0, reason = 'network failure';
      try {
        let response;
        try {
          response = await fetchImpl(endpoint, { method: 'POST',
            headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
            body: JSON.stringify({ jsonrpc: '2.0', id: requestId, method, params }),
            signal: controller.signal, redirect: 'error' });
        } catch { retryable = true; throw new Error(controller.signal.aborted ? 'timeout' : 'network failure'); }
        if (!response.ok) {
          retryable = response.status === 408 || response.status === 429 || response.status >= 500;
          const header = response.headers.get('retry-after');
          if (header) {
            const seconds = Number(header);
            retryAfter = Number.isFinite(seconds) ? seconds * 1000 : Date.parse(header) - now();
            if (!Number.isFinite(retryAfter)) retryAfter = 0;
            retryAfter = Math.max(0, Math.min(30000, retryAfter));
          }
          if (response.body) await response.body.cancel();
          throw new Error(`HTTP ${response.status}`);
        }
        let text;
        try { text = await response.text(); }
        catch { retryable = true; throw new Error(controller.signal.aborted ? 'timeout reading body' : 'network failure reading body'); }
        let result;
        try { result = JSON.parse(text); }
        catch { retryable = true; throw new Error('non-JSON response'); }
        if (!result || result.jsonrpc !== '2.0' || result.id !== requestId) throw new Error('invalid JSON-RPC envelope');
        // Application errors are not transport failures. Never blindly retry contract errors.
        if (result.error) throw new Error(`JSON-RPC application error (${result.error.code ?? 'unknown'})`);
        if (result.result === undefined) throw new Error('missing JSON-RPC result');
        return result.result;
      } catch (error) {
        reason = error.message;
        if (!retryable || attempt === attempts - 1) throw new Error(`RPC ${method}: ${reason}; stopped after ${attempt + 1} attempt(s)`);
      } finally { clearTimeout(timer); }
      const delay = Math.max(retryAfter, Math.min(8000, 500 * 2 ** attempt) + Math.floor(random() * 250));
      warn(`RPC ${method}: ${reason}; retry ${attempt + 2}/${attempts} in ${delay}ms`);
      await sleep(delay);
    }
    throw new Error('RPC retry budget exhausted');
  };
}

async function* canonicalBlocks(provider, headId, start, end, batchSize = 20) {
  for (let first = start; first <= end; first += batchSize) {
    const count = Math.min(batchSize, end - first + 1);
    const blocks = await provider.getBlocks(first, count, headId);
    if (!Array.isArray(blocks) || blocks.length !== count) throw new Error(`Incomplete block batch at ${first}`);
    blocks.sort((a, b) => Number(a.block_height) - Number(b.block_height));
    for (let i = 0; i < count; i++) {
      if (Number(blocks[i].block_height) !== first + i) throw new Error(`Invalid block batch coverage at ${first}`);
    }
    for (const block of blocks) yield block;
  }
}

module.exports = { createReadRpc, canonicalBlocks };
