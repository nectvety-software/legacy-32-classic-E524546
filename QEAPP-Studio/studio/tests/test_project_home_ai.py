from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest

from studio.core.ai_agent import AIAgent, ToolCall, parse_tool_blocks
from studio.core.project_library import ProjectLibrary
from studio.core.workspace import Workspace
from tools.qstudio import init_project


class ProjectLibraryTests(unittest.TestCase):
    def test_scan_preserves_recent_order_and_reads_metadata(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            first = root / 'first'
            second = root / 'second'
            init_project('text', first, 'first', 'First')
            init_project('lua', second, 'second', 'Second')
            library = ProjectLibrary(root, [second, first])
            records = library.scan()
            self.assertEqual([record.path.resolve() for record in records], [second.resolve(), first.resolve()])
            self.assertEqual(records[0].id, 'second')
            self.assertEqual(records[0].type, 'lua')
            self.assertTrue(records[0].recent)
            self.assertTrue(records[0].managed)


class AIAgentTests(unittest.TestCase):
    def test_context_and_tools_are_project_bounded(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / 'project'
            init_project('lua', root, 'agent_demo', 'Agent Demo')
            (root / 'private.pem').write_text('secret', encoding='utf-8')
            (root / 'build').mkdir()
            (root / 'build' / 'ignored.lua').write_text('return 1', encoding='utf-8')
            agent = AIAgent(settings_file=Path(folder) / 'ai.json')
            agent.set_project(Workspace(root))
            context = agent.context_report()
            self.assertIn('qeapp.project.json', context.text)
            self.assertNotIn('private.pem', context.text)
            self.assertNotIn('ignored.lua', context.text)
            result = agent.execute_tool(ToolCall('read', {'path': 'main.lua'}))
            self.assertTrue(result.ok)
            blocked = agent.execute_tool(ToolCall('read', {'path': '../outside.lua'}))
            self.assertFalse(blocked.ok)
            self.assertEqual(parse_tool_blocks('```qeapp-tool\n{"action":"glob","arguments":{"pattern":"*.lua"}}\n```')[0][0].action, 'glob')

    def test_settings_never_accept_api_key(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'ai.json'
            agent = AIAgent(settings_file=path)
            agent.configure(provider='ollama', model='llama3.2', base_url='http://127.0.0.1:11434')
            self.assertNotIn('api_key', path.read_text(encoding='utf-8').lower())
            self.assertEqual(json.loads(path.read_text(encoding='utf-8'))['provider'], 'ollama')


if __name__ == '__main__':
    unittest.main()
