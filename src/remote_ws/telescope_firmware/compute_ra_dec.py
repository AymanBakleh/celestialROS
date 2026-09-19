import datetime
try:
    import ephem
except Exception as e:
    print('ephem not installed:', e)
    raise SystemExit(1)

utc = datetime.datetime(2026, 3, 27, 5, 0, 0)
obs = ephem.Observer(); obs.lat='33.50917'; obs.lon='36.31167'; obs.date = utc

sun = ephem.Sun(obs)
moon = ephem.Moon(obs)
vega = ephem.star('Vega'); vega.compute(obs)

print('UTC:', utc)
for name, obj in [('Moon', moon), ('Sun', sun), ('Vega', vega)]:
    ra_deg = float(obj.ra) * 180.0 / ephem.pi
    dec_deg = float(obj.dec) * 180.0 / ephem.pi
    print(f'{name}: RA {ra_deg:.6f}°, DEC {dec_deg:.6f}°')
