"""Inert stdio LSP: diagnostics, hover, definitions and formatting share parser."""
import json
import re
import sys
from .source import parse, format_source, SourceError
from .checker import check
from .graph import operator_descriptor, elaborate


def serve(inp=None, out=None):
    inp, out = inp or sys.stdin.buffer, out or sys.stdout.buffer
    documents, shutdown = {}, False
    def send(payload):
        data = json.dumps(payload, ensure_ascii=False).encode()
        out.write(f'Content-Length: {len(data)}\r\n\r\n'.encode() + data)
        out.flush()
    def position(span):
        return {'line':max(0,span.get('line',1)-1),'character':max(0,span.get('column',1)-1)}
    def diagnose(uri):
        text = documents[uri]
        try:
            result = check(parse(text))
            diagnostics = result['diagnostics']
            if result['status'] == 'unsupported':
                diagnostics = diagnostics + [{'message':'Static checking has unresolved domain obligations; use explain for their scope.','severity':'warning'}]
        except SourceError as e:
            diagnostics = [e.as_dict()]
        converted=[]
        for d in diagnostics:
            start = position(d.get('span',{}))
            converted.append({'range':{'start':start,'end':dict(start,character=start['character']+1)},
                              'severity':2 if d.get('severity')=='warning' else 1,
                              'source':'etellis-u','code':d.get('code','obligation'), 'message':d['message']})
        send({'jsonrpc':'2.0','method':'textDocument/publishDiagnostics','params':{'uri':uri,'diagnostics':converted}})
    while True:
        headers={}
        while True:
            line=inp.readline()
            if not line:
                return 0 if shutdown else 1
            if line in (b'\r\n', b'\n'):
                break
            key, value=line.decode().split(':',1)
            headers[key.lower()]=value.strip()
        length=int(headers.get('content-length','0'))
        if not 0 < length <= 4_000_000:
            return 2
        raw=inp.read(length)
        if len(raw)!=length:
            return 2
        message=json.loads(raw)
        method, params=message.get('method'),message.get('params',{})
        result=None
        try:
            if method=='initialize':
                result={'serverInfo':{'name':'etellis-u','version':'0.1.0'},'capabilities':{
                    'textDocumentSync':1,'hoverProvider':True,'definitionProvider':True,'documentFormattingProvider':True}}
            elif method=='shutdown':
                shutdown=True
            elif method=='exit':
                return 0 if shutdown else 1
            elif method=='textDocument/didOpen':
                doc=params['textDocument']; documents[doc['uri']]=doc['text']; diagnose(doc['uri'])
            elif method=='textDocument/didChange':
                uri=params['textDocument']['uri']; documents[uri]=params['contentChanges'][-1]['text']; diagnose(uri)
            elif method=='textDocument/didClose':
                documents.pop(params['textDocument']['uri'],None)
            elif method in {'textDocument/hover','textDocument/definition'}:
                uri=params['textDocument']['uri']; text=documents.get(uri,''); lines=text.splitlines()
                p=params['position']; line=lines[p['line']] if p['line']<len(lines) else ''
                word=next((m.group() for m in re.finditer(r'[A-Za-z_][\w.]*',line) if m.start()<=p['character']<=m.end()),None)
                if word and method.endswith('hover'):
                    module=parse(text); graph=elaborate(module); descriptor=operator_descriptor(word)
                    result={'contents':{'kind':'markdown','value':f'**{word}**\n\nTheory: {descriptor["theory"]}; region: {descriptor["region_policy"]}.\n\nLaw status: {descriptor["proof_status"]}.\n\nProfile and resource obligations: {len(graph["obligations"])}. Run `etellis-u explain` for full contracts.'}}
                elif word:
                    module=parse(text)
                    definition=next((d for d in module['definitions'] if d['name']==word),None)
                    if definition:
                        start=position(definition.get('span',{})); result={'uri':uri,'range':{'start':start,'end':start}}
            elif method=='textDocument/formatting':
                text=documents.get(params['textDocument']['uri'],'')
                lines=text.split('\n')
                result=[{'range':{'start':{'line':0,'character':0},'end':{'line':len(lines)-1,'character':len(lines[-1])}},'newText':format_source(text)}]
            elif method not in {'initialized', '$/cancelRequest'} and 'id' in message:
                send({'jsonrpc':'2.0','id':message['id'],'error':{'code':-32601,'message':'Method not supported'}})
                continue
            if 'id' in message:
                send({'jsonrpc':'2.0','id':message['id'],'result':result})
        except (ValueError, KeyError, IndexError) as error:
            if 'id' in message:
                send({'jsonrpc':'2.0','id':message['id'],'error':{'code':-32602,'message':str(error)}})
