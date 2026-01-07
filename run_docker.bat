@echo off
docker compose build
docker compose run --rm cpp bash
