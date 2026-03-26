#!/bin/bash
# 1. Activate the virtual environment we built
source ~/.litellm_env/bin/activate

# 2. Set your validated Gemini key
export GEMINI_API_KEY="AIzaSyBDodhNTZmNm2_vJdOAL3QBvna4hdOSAI4"

# 3. Launch the bridge with the 2026 stable Pro model
echo "🚀 Starting Winnow64 AI Engine (Gemini 2.5 Pro)..."
litellm --model gemini/gemini-2.5-pro --drop_params